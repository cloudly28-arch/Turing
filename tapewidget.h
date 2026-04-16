#pragma once
#include <QWidget>
#include <QPainter>
#include <QTimer>
#include <cmath>
#include <algorithm>
#include "turingtape.h"

class TapeWidget : public QWidget {
    Q_OBJECT
public:
    explicit TapeWidget(TuringTape* tape, QWidget* parent = nullptr)
        : QWidget(parent), tape(tape)
    {
        connect(tape, &TuringTape::changed, this, &TapeWidget::onHeadMoved);
        setMinimumHeight(cellHeight + 30); // Запас места для треугольника сверху

        animTimer = new QTimer(this);
        animTimer->setInterval(16); // ~60 FPS
        connect(animTimer, &QTimer::timeout, this, &TapeWidget::animateStep);
    }

private slots:
    void onHeadMoved() {
        targetCursor = static_cast<float>(tape->headPosition());
        if (!animTimer->isActive()) animTimer->start();
    }

    void animateStep() {
        // 1. Плавное движение курсора
        float cursorDiff = targetCursor - cursorPos;
        if (std::abs(cursorDiff) < 0.001f) {
            cursorPos = targetCursor;
        } else {
            cursorPos += cursorDiff * 0.15f; // Коэффициент плавности
        }

        // 2. Авто-скролл ленты при приближении к краю
        float visibleCells = width() / cellWidth;
        float leftMargin  = 4.0f;
        float rightMargin = visibleCells - leftMargin;
        float posInView = cursorPos - viewOffset;
        float scrollDiff = 0.0f;

        if (posInView < leftMargin)      scrollDiff = posInView - leftMargin;
        else if (posInView > rightMargin) scrollDiff = posInView - rightMargin;

        if (std::abs(scrollDiff) > 0.001f) viewOffset += scrollDiff * 0.15f;

        // Останавливаем таймер, когда анимация завершилась
        if (std::abs(cursorDiff) < 0.001f && std::abs(scrollDiff) < 0.001f) {
            animTimer->stop();
        }
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing); // Сглаживание линий и фигур
        p.fillRect(rect(), Qt::white);
        p.setFont(QFont("Consolas", 11, QFont::Monospace));
        p.setPen(Qt::black);

        const QChar blank = tape->blankSymbol();
        float visibleCells = width() / cellWidth;

        // Рисуем только видимые + 1 запасную ячейку по краям
        int startIdx = static_cast<int>(viewOffset) - 1;
        int endIdx   = static_cast<int>(viewOffset + visibleCells) + 1;

        // 1. Фиксированная сетка ячеек
        for (int i = startIdx; i <= endIdx; ++i) {
            float x = (i - viewOffset) * cellWidth;
            float y = (height() - cellHeight) / 2.0f;

            p.drawRect(qRound(x), qRound(y), cellWidth, cellHeight);

            QChar sym = tape->symbolAt(i);
            QString text = (sym == blank) ? QString("_") : QString(sym);
            p.drawText(qRound(x), qRound(y), cellWidth, cellHeight, Qt::AlignCenter, text);
        }

        // 2. Треугольный курсор
        float cellLeftX   = (cursorPos - viewOffset) * cellWidth;
        float cellCenterX = cellLeftX + cellWidth / 2.0f;
        float y           = (height() - cellHeight) / 2.0f;

        // НАСТРОЙКА ВЫСОТЫ: меняйте это число, чтобы поднять/опустить курсор
        // Отрицательное значение поднимает вверх от верхней границы ячейки
        float cursorOffsetY = -15.0f;

        float triHalfW = cellWidth * 0.3f;
        float triH     = cellHeight * 0.45f;

        // Верхняя линия треугольника
        float topY = y + cursorOffsetY;

        QPolygonF triangle;
        triangle << QPointF(cellCenterX - triHalfW, topY)          // Левый верхний угол
                 << QPointF(cellCenterX + triHalfW, topY)          // Правый верхний угол
                 << QPointF(cellCenterX, topY + triH);             // Нижнее остриё

        p.save();
        p.setPen(QPen(QColor(200, 50, 50), 2));
        p.setBrush(QColor(255, 80, 80, 160));
        p.drawPolygon(triangle);
        p.restore();
    }

private:
    TuringTape* tape;
    QTimer* animTimer = nullptr;
    float cursorPos   = 0.0f;
    float targetCursor = 0.0f;
    float viewOffset  = 0.0f; // Плавное смещение камеры
    const int cellWidth  = 40;
    const int cellHeight = 30;
};
