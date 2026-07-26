#pragma once

#include <QWidget>

namespace wavely::ui {

/// Frameless, translucent top-level window hosting the Wavely overlay widget. Phase 0 renders
/// a placeholder rounded panel; playback UI, presets and interactions land in later phases.
class MainWidget : public QWidget {
public:
    explicit MainWidget(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
};

} // namespace wavely::ui
