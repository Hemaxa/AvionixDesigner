#!/usr/bin/env python3
"""Побайтовое сравнение BIN редактора с All_together.py без подключения UART.

Загружаются только выбранные определения протокола и XML-загрузчика.
Главный цикл, pyserial, keyboard, numpy и OpenCV не исполняются.
"""
import argparse
import ast
import __future__
import base64
import hashlib
import json
import logging
import math
import os
from pathlib import Path
import random
import subprocess
import struct
import xml.etree.ElementTree as ET
import zlib

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


def editor_settings(root, fields):
    """Полный проект для проверки загрузки; аппаратные слова остаются эталонными."""
    editor = ET.SubElement(root, 'editor', version='1')
    workspace = ET.SubElement(editor, 'workspace', {
        'canvas-width': root.get('width'), 'canvas-height': root.get('height'),
        'background': '#ff000000', 'show-grid': 'false', 'grid-step': '10',
        'grid-color': '#3778b4c8', 'snap-to-canvas': 'true', 'snap-to-grid': 'false',
        'snap-to-objects': 'false', 'canvas-guide-color': '#d2ff5c7a',
        'grid-guide-color': '#d256d3ff', 'object-guide-color': '#dcffca58'})
    ET.SubElement(workspace, 'project-name').text = root.get('name')
    ET.SubElement(workspace, 'view', {'scale': '1', 'offset-x': '0', 'offset-y': '0',
                                    'proportional-resize': 'false', 'rotation-snapping': 'false'})
    layers = ET.SubElement(editor, 'layers', {'active-id': '1', 'show-all': 'false', 'next-id': '2'})
    layer = ET.SubElement(layers, 'layer', {'id': '1', 'selection-color': '#ff00c8f0'})
    ET.SubElement(layer, 'name').text = 'Layer 1'
    scene = ET.SubElement(editor, 'scene', order='front-to-back')
    ET.SubElement(editor, 'groups', {'next-id': '1'})
    resources = ET.SubElement(editor, 'resources')

    def color(bgr, alpha=255):
        return f'#{alpha:02x}{bgr & 255:02x}{(bgr >> 8) & 255:02x}{(bgr >> 16) & 255:02x}'

    def mask(parent, width, height, values):
        def chunk(tag, data):
            return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data))
        rows = b''.join(b'\0' + b''.join(bytes((255, 255, 255, value * 255 // 7))
                                       for value in values[y * width:(y + 1) * width]) for y in range(height))
        png = (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
               + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))
        resource_id = f'resource-{len(resources) + 1}'
        resource = ET.SubElement(resources, 'binary', {'id': resource_id, 'encoding': 'base64',
                                'media-type': 'image/png', 'sha256': hashlib.sha256(png).hexdigest()})
        resource.text = base64.b64encode(png).decode('ascii')
        ET.SubElement(parent, 'mask', ref=resource_id)

    for index, hardware in enumerate(reversed(list(root.find('objects'))), 1):
        tag = hardware.tag
        obj = ET.SubElement(scene, 'object', {'id': f'object-{index}', 'type': 'rectangle' if tag == 'rectangle_a' else tag,
                                            'export-tag': tag, 'layer-id': '1'})
        ET.SubElement(obj, 'name')
        flags = ET.SubElement(obj, 'flags', {'visible': 'true', 'selection-locked': 'false',
                              'export-enabled': 'true', 'resize-locked': 'false', 'imported-hardware': 'false'})
        ET.SubElement(flags, 'restriction-hint')
        params = ET.SubElement(obj, 'parameters')
        if tag == 'layer3d':
            params.set('left-anchored', 'false')
            ET.SubElement(params, 'init').text = hardware.findtext('init')
            continue
        decoded = []
        for init in hardware.findall('init'):
            value = int(init.text, 16)
            values = {name: (value >> offset) & ((1 << size) - 1) for name, offset, size in fields[tag]}
            if tag == 'rotationobject':
                for name in ('left', 'top', 'right', 'bottom', 'sin', 'cos'):
                    size = next(size for field, _, size in fields[tag] if field == name)
                    if values[name] & (1 << (size - 1)): values[name] -= 1 << size
            decoded.append(values)
        v = decoded[0]
        if tag.startswith('rectangle'):
            alpha = math.floor((v['alph'] + 1) * 255 / 8 + 0.5) if tag == 'rectangle_a' else 255
            params.attrib.update({key: str(value) for key, value in dict(
                enabled='true', x=v['x0'], y=v['y0'], width=v['w'], height=v['h'], fill=color(v['color'], alpha),
                stroke=color(v['colorb']), **{'stroke-width': v['a'], 'alpha': alpha,
                'explicit-alpha': 'true' if tag == 'rectangle_a' else 'false'}).items()})
        elif tag == 'rotationobject':
            values = {'enabled': 'true', 'left': v['left'], 'top': v['top'], 'right': v['right'], 'bottom': v['bottom'],
                      'center-x': v['xrot'], 'center-y': v['yrot'], 'sin': v['sin'], 'cos': v['cos'], 'color': color(v['color'])}
            params.attrib.update({key: str(value) for key, value in values.items()})
            data = hardware.find('data')
            mask(params, int(data.get('width')), int(data.get('height')), [int(x) for x in data.text.split(',')])
            ET.SubElement(ET.SubElement(params, 'hardware-layers'), 'layer', params.attrib.copy())
        else:
            params.set('group-number', hardware.get('nomber'))
            states, masks = ET.SubElement(params, 'states'), ET.SubElement(params, 'masks')
            data = [int(x) for x in hardware.findtext('data').split(',')]
            for v in decoded:
                ET.SubElement(states, 'state', {key: str(value) for key, value in dict(
                    x=v['x'], y=v['y'], width=v['w'], height=v['h'], address=v['addr'], color=color(v['color']),
                    enabled='true' if v['enb'] else 'false').items()})
                mask(masks, v['w'], v['h'], data[v['addr']:v['addr'] + v['w'] * v['h']])


def fixture(path, ref, rotation=(32768, 56756)):
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
        ('rotationobject',dict(enb=1,color=0xaabbcc,xrot=350,yrot=240,left=-5,top=-6,right=5,bottom=5,sq=2,sin=rotation[0],cos=rotation[1])),
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
    editor_settings(root, fields)
    ET.indent(root)
    ET.ElementTree(root).write(path,encoding='utf-8',xml_declaration=True)
    return pose


class Capture:
    """Подменяет порт при проверке и собирает исходные байты без переупаковки."""
    def __init__(self): self.data = bytearray()
    def write(self, data): self.data.extend(data); return len(data)


def run_editor(app, option, source, output, expected_returncode=0):
    if not source.is_file():
        raise FileNotFoundError(f'Input XML does not exist: {source}')
    output.unlink(missing_ok=True)
    command = [str(app), option, str(source), str(output)]
    result = subprocess.run(command, capture_output=True, text=True,
                            encoding='utf-8', errors='replace',
                            env={**os.environ, 'QT_QPA_PLATFORM':'offscreen'})
    diagnostics = (f'Command: {command!r}\nExit code: {result.returncode}\n'
                   f'STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}')
    log = output.with_suffix(output.suffix + '.log')
    log.write_text(diagnostics, encoding='utf-8')
    if result.returncode != expected_returncode:
        raise RuntimeError(f'Expected exit code {expected_returncode}. '
                           f'Diagnostics saved to {log}\n{diagnostics}')
    return result


def verify_rejected_xml(app, reference_xml, output_dir):
    """Создаем неправильные XML из свежего примера, без локальных tests/*.xml."""
    def check(name, root):
        source = output_dir/f'reject-{name}.xml'
        output = output_dir/f'reject-{name}.bin'
        ET.indent(root)
        ET.ElementTree(root).write(source, encoding='utf-8', xml_declaration=True)
        result = run_editor(app, '--export-bin', source, output, expected_returncode=1)
        if not result.stderr.strip() or output.exists():
            raise AssertionError(f'{name}: invalid XML must report an error without producing a BIN')
        print(f'{name}: unsupported XML rejected with an error')

    for canonical, alias in [('rotationobject', 'rotation_object'),
                             ('staticgroup', 'static_group'),
                             ('layer3d', 'three_d_view')]:
        root = ET.parse(reference_xml).getroot()
        root.find(f'parameters/{canonical}').tag = alias
        root.find(f'objects/{canonical}').tag = alias
        check(alias, root)

    root = ET.parse(reference_xml).getroot()
    root.find('parameters/layer3d/yscreen').tag = 'pitch'
    check('old-screen-field', root)

    root = ET.parse(reference_xml).getroot()
    root.find('parameters/layer3d/x').set('size', '17')
    check('old-coordinate-layout', root)


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
    cases = {'right': (32768, 56756), 'negative-sin': (-32768, 56756),
             'negative-cos': (32768, -56756), 'negative-both': (-32768, -56756)}
    for name, rotation in cases.items():
        xml=args.output/f'reference-{name}.xml'
        pose=fixture(xml,ref,rotation)
        capture=Capture();loader=ref['VbDataLoader'](capture,ref['VbXML'](str(xml)).get_dict())
        loader.load_objects();loader.load_3d('right');capture.write(ref['pack_layer3d'](pose))
        expected=bytes(capture.data[1:]) # Начальный 0x00 относится к UART-сеансу, а не BIN.
        expected_file=args.output/f'python-{name}.bin';expected_file.write_bytes(expected)
        actual_file=args.output/f'editor-{name}.bin'
        run_editor(args.app,'--export-bin',xml,actual_file);actual=actual_file.read_bytes()
        if actual!=expected:
            first=next((i for i,(a,b) in enumerate(zip(actual,expected)) if a!=b),min(len(actual),len(expected)))
            raise AssertionError(f'{name}: BIN differs at byte {first}, editor={len(actual)}, reference={len(expected)}')
        print(f'{name}: identical complete BIN, {len(actual)} bytes, {len(frames(actual))} packets')
    verify_rejected_xml(args.app, args.output/'reference-right.xml', args.output)
    generate_vectors(ref,args.output/'position-vectors.json')
    print('Generated Python position vectors for AvionixProtocolTests.')


if __name__=='__main__': main()
