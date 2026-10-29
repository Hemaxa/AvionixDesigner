#!/usr/bin/env python3
"""Побайтовое сравнение BIN редактора с All_together.py без подключения UART.

Загружаются только выбранные определения протокола и XML-загрузчика.
Главный цикл, pyserial, keyboard, numpy и OpenCV не исполняются.
"""
import argparse
import ast
import hashlib
import platform
import __future__
import json
import logging
import math
import os
from pathlib import Path
import random
import subprocess
import xml.etree.ElementTree as ET

TEST_DIR = Path(__file__).resolve().parent
ROOT = TEST_DIR.parent.parent


def oracle(path):
    functions = {
        'fletcher32', '_be', 'build_packet', '_memory_payload', 'pack_modification',
        'pack_modification_static', 'pack_parameters', 'pack_layer3d', 'course_to_raw',
        'meters_to_height_raw', 'move_by_speed', 'pack_150bit', 'setup_all', '_clean', 'to_signed',
    }
    classes = {'VbXML', 'VbDataWorks', 'VbDataLoader'}
    constants = {
        'MAGIC', 'FCS_SIZE', 'MSG_MODIFY_ROT', 'MSG_PARAMS', 'MSG_MODIFY_STATIC',
        'MSG_LAYER3D', 'OBJECT_TEMPLATES', '_SIGNED_FIELDS', '_COLOR_FIELDS',
        '_SIMPLE_TYPES', 'STATIC_CHUNK_SIZE', 'COURSE_MODULUS', 'COURSE_SCALE',
        'HEIGHT_SCALE', 'HEIGHT_RAW_MIN', 'HEIGHT_RAW_MAX', 'COORD_MASK',
        'COURSE_MASK', 'HEIGHT_MASK', 'METERS_PER_POINT', 'SUBUNITS_PER_POINT',
    }
    selected = []
    for node in ast.parse(path.read_text(encoding='utf-8')).body:
        if isinstance(node, ast.FunctionDef) and node.name in functions:
            selected.append(node)
        elif isinstance(node, ast.ClassDef) and node.name in classes:
            selected.append(node)
        elif isinstance(node, ast.Assign):
            names = {n.id for t in node.targets for n in ast.walk(t) if isinstance(n, ast.Name)}
            if names and names <= constants:
                selected.append(node)
    namespace = {'ET': ET, 'logger': logging.getLogger('reference'), 'math': math}
    exec(compile(ast.Module(body=selected, type_ignores=[]), str(path), 'exec',
                 flags=__future__.annotations.compiler_flag), namespace)
    return namespace


def frames(data):
    result, offset = [], 0
    while offset < len(data):
        if data[offset:offset + 4] != b'SAIN' or len(data) - offset < 11:
            raise AssertionError(f'Invalid packet at byte {offset}')
        length = int.from_bytes(data[offset + 4:offset + 6], 'big')
        packet = data[offset:offset + 6 + length]
        if length < 5 or len(packet) != 6 + length:
            raise AssertionError(f'Truncated packet at byte {offset}')
        result.append(packet)
        offset += len(packet)
    return result


def word(fields, values):
    result = 0
    for name, offset, width in fields:
        result |= (values.get(name, 0) & ((1 << width) - 1)) << offset
    return f'{result:038X}'


def fixture(path, ref):
    root = ET.Element('project', name='UART reference', width='1024', height='768', bgcolor='#0')
    parameters, objects = ET.SubElement(root, 'parameters'), ET.SubElement(root, 'objects')
    fields = {
        'layer3d': [('x',0,19),('y',19,19),('course',38,19),('height',57,17),('yscreen',74,10),('padding',84,66)],
    }
    for name in ['rectangle', 'rectangle_a', 'rotationobject', 'staticgroup']:
        fields[name] = [(k,v['offset'],v['size']) for k,v in ref['OBJECT_TEMPLATES'][name].items()]
        # Эталон называет поле alpha; схема редактора использует имя alph.
        fields[name] = [('alph' if k == 'alpha' else k, o, w) for k,o,w in fields[name]]
        end = max(o+w for _,o,w in fields[name]); fields[name].append(('padding',end,150-end))
    for name, schema in fields.items():
        tag = ET.SubElement(parameters, name)
        for field, offset, width in schema:
            ET.SubElement(tag, field, offset=str(offset), size=str(width))
    layer = ET.SubElement(objects, 'layer3d')
    pose = ref['pack_150bit'](131104, 65, ref['course_to_raw'](45.05), ref['meters_to_height_raw'](8000.25), 1023)
    ET.SubElement(layer, 'init').text = f'{int.from_bytes(pose,"little"):038X}'
    for name, values in [
        ('rectangle',dict(enb=1,color=0x563412,colorb=0xabcdef,x0=15,y0=20,w=120,h=80,a=2)),
        ('rectangle_a',dict(enb=1,color=0x00ff88,colorb=0x112233,alph=3,x0=160,y0=35,w=100,h=60,a=1)),
        ('rotationobject',dict(enb=1,color=0xaabbcc,xrot=350,yrot=240,left=-5,top=-6,right=5,bottom=5,sq=2,sin=32768,cos=56756)),
        ('staticgroup',dict(enb=1,color=0xfedcba,x=400,y=300,w=32,h=32,addr=0)),
    ]:
        obj = ET.SubElement(objects, name)
        ET.SubElement(obj,'init').text = word(fields[name],values)
        if name == 'rotationobject':
            ET.SubElement(obj,'data',width='10',height='11').text = ', '.join(str((i*3+i//10)%8) for i in range(110))
        if name == 'staticgroup':
            obj.set('nomber','2')
            ET.SubElement(obj,'init',index='2').text = word(fields[name],{**values,'enb':0,'x':450,'addr':1024})
            ET.SubElement(obj,'data').text = ', '.join(str((i*5+i//32)%8) for i in range(2048))
    ET.indent(root)
    ET.ElementTree(root).write(path,encoding='utf-8',xml_declaration=True)
    return pose


class Capture:
    """Подменяет порт при проверке и собирает исходные байты без переупаковки."""
    def __init__(self): self.data = bytearray()
    def write(self, data): self.data.extend(data); return len(data)


def run_editor(app, option, source, output):
    subprocess.run([str(app), option, str(source), str(output)], check=True,
                   env={**os.environ,'QT_QPA_PLATFORM':'offscreen'})


def generate_vectors(ref, output):
    rng = random.Random(20261027)
    vectors = []
    for i in range(1000):
        course = rng.uniform(-720,720)
        height = rng.uniform(-20000,20000)
        x,y,screen = rng.randrange(524288),rng.randrange(524288),rng.randrange(1024)
        raw_course,raw_height = ref['course_to_raw'](course),ref['meters_to_height_raw'](height)
        payload = ref['pack_150bit'](y,x,raw_course,raw_height,screen)
        vectors.append(dict(x=x,y=y,course=course,height=height,yscreen=screen,
                            course_raw=raw_course,height_raw=raw_height,payload=payload.hex(),
                            packet=ref['pack_layer3d'](payload).hex()))
    for value in [-2.5,-1.5,-0.5,0.5,1.5,2.5,100.5,524286.5]:
        course=value/ref['COURSE_SCALE'];height=value/4
        payload=ref['pack_150bit'](0,0,ref['course_to_raw'](course),ref['meters_to_height_raw'](height),1023)
        vectors.append(dict(x=0,y=0,course=course,height=height,yscreen=1023,
                            course_raw=ref['course_to_raw'](course),height_raw=ref['meters_to_height_raw'](height),
                            payload=payload.hex(),packet=ref['pack_layer3d'](payload).hex()))
    output.write_text(json.dumps(vectors,indent=2))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app',type=Path,required=True)
    parser.add_argument('--reference',type=Path,default=TEST_DIR/'All_together.py')
    parser.add_argument('--output',type=Path,default=ROOT/'build/verification')
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    ref=oracle(args.reference)
    for side in ['right']:
        xml=args.output/f'reference-{side}.xml'
        pose=fixture(xml,ref)
        capture=Capture();loader=ref['VbDataLoader'](capture,ref['VbXML'](str(xml)).get_dict())
        loader.load_objects();loader.load_3d(side);capture.write(ref['pack_layer3d'](pose))
        expected=bytes(capture.data[1:]) # Начальный 0x00 относится к UART-сеансу, а не BIN.
        expected_file=args.output/f'python-{side}.bin';expected_file.write_bytes(expected)
        actual_file=args.output/f'editor-{side}.bin'
        run_editor(args.app,'--export-bin',xml,actual_file);actual=actual_file.read_bytes()
        if actual!=expected:
            first=next((i for i,(a,b) in enumerate(zip(actual,expected)) if a!=b),min(len(actual),len(expected)))
            raise AssertionError(f'{side}: BIN differs at byte {first}, editor={len(actual)}, reference={len(expected)}')
        print(f'{side}: identical complete BIN, {len(actual)} bytes, {len(frames(actual))} packets')
    manifest = json.loads((TEST_DIR/'non3d_baseline.json').read_text(encoding='utf-8'))
    compare_hash = platform.system() == manifest['_platform']
    for filename in ['test.xml', 'test1.xml', 'test2.xml']:
        actual_file = args.output/('regression-'+Path(filename).stem+'.bin')
        run_editor(args.app, '--export-bin', ROOT/'tests'/filename, actual_file)
        packets = [packet for packet in frames(actual_file.read_bytes()) if packet[6] != 6]
        digest = hashlib.sha256(b''.join(packets)).hexdigest()
        baseline = manifest[filename]
        assert len(packets) == baseline['packets'] and (not compare_hash or digest == baseline['sha256']), filename
        print(f'{filename}: canonical XML, {len(packets)} unchanged non-3D packets')
    for filename in ['image.xml', 'test3.xml', 'test4.xml']:
        result = subprocess.run([str(args.app), '--export-bin', str(ROOT/'tests'/filename), str(args.output/'rejected.bin')],
                                capture_output=True, env={**os.environ, 'QT_QPA_PLATFORM':'offscreen'})
        assert result.returncode != 0, f'Legacy XML unexpectedly accepted: {filename}'
        print(f'{filename}: unsupported old XML rejected')
    generate_vectors(ref,args.output/'position-vectors.json')
    print('Generated Python position vectors for AvionixProtocolTests.')


if __name__=='__main__': main()
