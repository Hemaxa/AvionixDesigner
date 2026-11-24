// StaticImageEditorPanel — постоянное окно с черновиками выбранных статических изображений.
#pragma once

#include "BaseObject.h"
#include "BasePanel.h"
#include "editor/StaticImageDocument.h"
#include <QHash>
#include <QSet>

class StaticImageCanvas;
class QLabel;
class QPushButton;
class QToolButton;
class QListWidget;
class QStackedWidget;
class QPropertyAnimation;
class QComboBox;
class QSpinBox;
class QSlider;

// StaticImageEditorPanel — обзор разрезов и выезжающая страница масок; один Apply в историю сцены.
class StaticImageEditorPanel : public BasePanel
{
    Q_OBJECT
  public:
    explicit StaticImageEditorPanel(QWidget *parent = nullptr);
    ~StaticImageEditorPanel() override;
    void setSelectedObjects(const QList<int> &indexes);
    void beginEditing();
    void cancelEditing();
    bool applyEditing();
    const StaticImageDocument *draft() const;
    bool editing() const;
  signals:
    void editingStateChanged();
    void objectApplied(int index);

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    // Session — отдельный черновик и локальная история, переживающие смену выделения.
    struct Session
    {
        QSharedPointer<BaseObject> source;
        StaticImageDocument document;
        QList<StaticImageDocument> undo, redo;
        bool editing = false;
        QImage beforeImage;
        QByteArray sourceKey;
    };
    void refresh(bool rebuildList = true);
    void saveHistory();
    void undoDraft(bool redo = false);
    void openRegion(int region);
    void showOverview();
    void animatePage(bool detail);
    void mergeMasks(const QList<int> &indexes, int target);
    void selectMasks(const QList<int> &indexes, int target);
    void changeColor();
    void reorderMasks();
    void updateMaskActions();
    void moveMasks(int direction, bool edge = false);
    void updateHint();
    void updateCutControls(bool preview = false);
    void chooseTool(int mode);
    void showBefore(bool before);
    QSet<int> selectedMasks() const;
    void showError(const QString &message);
    void pruneSessions();
    void trimHistory();
    QHash<BaseObject *, QSharedPointer<Session>> m_sessions;
    QSharedPointer<Session> m_session;
    QStackedWidget *m_pages = nullptr;
    QWidget *m_overview = nullptr, *m_detail = nullptr;
    StaticImageCanvas *m_canvas = nullptr, *m_partCanvas = nullptr;
    QLabel *m_name = nullptr, *m_partName = nullptr, *m_hint = nullptr, *m_statsEmpty = nullptr;
    QWidget *m_stats = nullptr;
    QHash<QString, QLabel *> m_statValues;
    QPushButton *m_begin = nullptr, *m_apply = nullptr, *m_cancel = nullptr, *m_color = nullptr,
                *m_merge = nullptr;
    QListWidget *m_masks = nullptr;
    QToolButton *m_mergeTool = nullptr, *m_empty = nullptr, *m_undo = nullptr, *m_redo = nullptr;
    QToolButton *m_top = nullptr, *m_up = nullptr, *m_down = nullptr, *m_bottom = nullptr;
    QToolButton *m_selectTool = nullptr, *m_partTool = nullptr, *m_extendTool = nullptr, *m_eraser = nullptr,
                *m_hand = nullptr, *m_isolate = nullptr, *m_find = nullptr, *m_before = nullptr,
                *m_after = nullptr, *m_split = nullptr;
    QComboBox *m_cutRegion = nullptr, *m_cutAxis = nullptr;
    QSpinBox *m_cutPosition = nullptr, *m_brushSize = nullptr;
    QSlider *m_brushSlider = nullptr;
    QLabel *m_maskInfo = nullptr, *m_coordinateLabel = nullptr;
    QPropertyAnimation *m_transition = nullptr;
    int m_part = -1;
    bool m_updating = false, m_applying = false, m_inDetail = false;
    bool m_reorderPending = false;
    bool m_showBefore = false;
    int m_toolMode = 0;
};
