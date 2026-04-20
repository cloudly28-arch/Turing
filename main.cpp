#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QVector>
#include <QTimer>
#include <QtMath>
#include <QRegularExpression>
#include "tapewidget.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QMainWindow window;
    window.setWindowTitle("Эмулятор Машины Тьюринга");
    window.resize(950, 750);

    auto* tape = new TuringTape(&window);
    auto* tapeWidget = new TapeWidget(tape, &window);

    // === АЛФАВИТЫ ===
    auto* editAlpha = new QLineEdit;
    auto* editAdd   = new QLineEdit;
    auto* btnAlpha  = new QPushButton("Задать алфавиты");

    auto* alphaLayout = new QHBoxLayout;
    alphaLayout->addWidget(new QLabel("Алфавит ленты:"));
    alphaLayout->addWidget(editAlpha);
    alphaLayout->addWidget(new QLabel("Доп. символы:"));
    alphaLayout->addWidget(editAdd);
    alphaLayout->addWidget(btnAlpha);

    // === СОСТОЯНИЯ ===
    QStringList states = {"q0"};
    auto* statesLabel = new QLabel("Состояния: q0");
    auto* btnAddState = new QPushButton("+");
    auto* btnRemState = new QPushButton("-");
    btnAddState->setFixedWidth(30); btnRemState->setFixedWidth(30);
    btnRemState->setToolTip("Удалить последнее состояние");
    auto* statesLayout = new QHBoxLayout;
    statesLayout->addWidget(btnAddState); statesLayout->addWidget(btnRemState);
    statesLayout->addWidget(statesLabel); statesLayout->addStretch();

    // === ТАБЛИЦА ===
    auto* tableRules = new QTableWidget(0, 0);
    tableRules->setEditTriggers(QAbstractItemView::DoubleClicked);
    tableRules->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tableRules->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    tableRules->setEnabled(false); tableRules->setAlternatingRowColors(true);

    // === ВХОДНОЕ СЛОВО ===
    auto* editWord = new QLineEdit;
    auto* btnWord  = new QPushButton("Задать строку");
    editWord->setEnabled(false); btnWord->setEnabled(false);
    auto* wordLayout = new QHBoxLayout;
    wordLayout->addWidget(new QLabel("Входное слово:"));
    wordLayout->addWidget(editWord); wordLayout->addWidget(btnWord);
    wordLayout->addStretch();

    // === УПРАВЛЕНИЕ ===
    auto* btnStep      = new QPushButton("▶ Шаг");
    auto* btnStart     = new QPushButton("▶▶ Запустить");
    auto* btnStop      = new QPushButton("⏹ Остановить");
    auto* btnReset     = new QPushButton("🔄 Сброс");
    auto* btnFaster    = new QPushButton("⏩ Быстрее");
    auto* btnSlower    = new QPushButton("⏪ Медленнее");
    auto* btnClearTable= new QPushButton("🗑 Очистить таблицу");

    auto* controlLayout = new QHBoxLayout;
    controlLayout->addWidget(btnStep); controlLayout->addWidget(btnStart);
    controlLayout->addWidget(btnStop); controlLayout->addWidget(btnReset);
    controlLayout->addWidget(btnClearTable);
    controlLayout->addWidget(btnFaster); controlLayout->addWidget(btnSlower);
    controlLayout->addStretch();

    // === КОМПОНОВКА ===
    auto* mainLayout = new QVBoxLayout;
    mainLayout->addLayout(alphaLayout); mainLayout->addLayout(statesLayout);
    mainLayout->addWidget(tableRules, 2);
    mainLayout->addWidget(new QLabel("Формат: Символ;Направление;Состояние | Пробел = не менять | ! в напр. = стоп | ! на ленте = стоп"));
    mainLayout->addLayout(wordLayout); mainLayout->addLayout(controlLayout);
    mainLayout->addWidget(tapeWidget, 3);

    auto* central = new QWidget; central->setLayout(mainLayout);
    window.setCentralWidget(central);

    // === ПЕРЕМЕННЫЕ ===
    QStringList currentSymbols;
    QString currentState = "q0";
    QString initialWord = "";
    bool isRunning = false;
    int stepInterval = 800;
    QTimer* runTimer = new QTimer(&window);
    runTimer->setSingleShot(false);
    runTimer->setInterval(stepInterval);

    // === ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ===
    auto lockUI = [&](bool locked) {
        editAlpha->setEnabled(!locked); editAdd->setEnabled(!locked);
        btnAlpha->setEnabled(!locked); btnAddState->setEnabled(!locked);
        btnRemState->setEnabled(!locked); editWord->setEnabled(!locked);
        btnWord->setEnabled(!locked); btnStep->setEnabled(!locked && !isRunning);
        btnStart->setEnabled(!locked);
        btnClearTable->setEnabled(!locked);
    };

    // 🔥 Подсветка КОНКРЕТНОЙ ячейки (row, col)
    auto highlightCell = [&](int row, int col) {
        for (int r = 0; r < tableRules->rowCount(); ++r) {
            for (int c = 0; c < tableRules->columnCount(); ++c) {
                auto* item = tableRules->item(r, c);
                if (item) item->setData(Qt::BackgroundRole, QVariant());
            }
        }
        if (row != -1 && col != -1) {
            auto* item = tableRules->item(row, col);
            if (item) item->setData(Qt::BackgroundRole, QColor(135, 206, 250)); // Голубая подсветка
        }
    };

    auto stopExecution = [&](const QString& reason) {
        isRunning = false;
        runTimer->stop();
        lockUI(false);
        highlightCell(-1, -1); // Убираем подсветку
        QMessageBox::information(&window, "Остановка", reason);
    };

    auto validateTable = [&](bool requireStop = true) -> bool {
        bool hasStop = false;
        QRegularExpression rulePattern(R"(^[^;]*;[^;]*;[^;]*$)");

        for (int r = 0; r < tableRules->rowCount(); ++r) {
            for (int c = 0; c < tableRules->columnCount(); ++c) {
                auto* item = tableRules->item(r, c);
                if (!item) continue;
                QString txt = item->text();
                if (txt.isEmpty()) continue;

                if (txt.contains('!')) hasStop = true;

                QString normalized = txt.replace(',', ';');
                QStringList parts = normalized.split(';');
                if (parts.size() != 3 || !rulePattern.match(normalized).hasMatch()) {
                    QMessageBox::warning(&window, "Ошибка формата ячейки",
                                         QString("Ячейка [%1, %2]: неверный формат.\nОжидается: Символ;Направление(L/R/S/!);Состояние").arg(r+1).arg(c+1));
                    return false;
                }
                QString dir = parts[1].trimmed().toUpper();
                if (!dir.isEmpty() && dir != "L" && dir != "R" && dir != "S" && dir != "!") {
                    QMessageBox::warning(&window, "Ошибка направления",
                                         QString("Ячейка [%1, %2]: направление должно быть L, R, S, ! или пусто.").arg(r+1).arg(c+1));
                    return false;
                }
            }
        }

        if (requireStop && !hasStop) {
            QMessageBox::warning(&window, "Ошибка валидации",
                                 "В таблице не задано ни одного условия остановки (символ '!').\nСогласно ТЗ, автозапуск запрещён.");
            return false;
        }
        return true;
    };

    auto executeStep = [&]() {
        QChar currentSym = tape->read();

        // 🛑 1. Стоп при чтении '!' с ленты
        if (currentSym == '!') {
            stopExecution("Головка прочитала символ '!'. Аварийная остановка."); return;
        }

        int stateRow = states.indexOf(currentState);
        int symCol = currentSymbols.indexOf(QString(currentSym));
        highlightCell(stateRow, symCol); // Подсвечиваем текущую ячейку

        if (stateRow == -1 || symCol == -1) {
            stopExecution("Нет правила для текущего состояния и символа."); return;
        }

        auto* cell = tableRules->item(stateRow, symCol);
        if (!cell || cell->text().isEmpty()) {
            stopExecution("Ячейка перехода пуста. Машина остановлена."); return;
        }

        QString cellText = cell->text();
        QString normalized = cellText.replace(',', ';');
        QStringList parts = normalized.split(';');
        if (parts.size() != 3) { stopExecution("Ошибка парсинга ячейки."); return; }

        bool writeEmpty = parts[0].trimmed().isEmpty();
        QChar writeSym = writeEmpty ? currentSym : parts[0].trimmed()[0];

        QString dirStr = parts[1].trimmed().toUpper();
        QString nextState = parts[2].trimmed().isEmpty() ? currentState : parts[2].trimmed();

        // ✅ 2. СНАЧАЛА ЗАПИСЫВАЕМ СИМВОЛ (даже если потом будет стоп)
        tape->write(writeSym);

        // ✅ 3. ПРОВЕРЯЕМ НА СТОП В НАПРАВЛЕНИИ
        if (dirStr == "!") {
            stopExecution("В поле направления указан '!'. Символ записан, выполнение остановлено.");
            return;
        }

        // ✅ 4. ЕСЛИ НЕ СТОП: двигаем головку и меняем состояние
        QString dir = dirStr.isEmpty() ? "S" : dirStr;
        if (dir == "L") tape->moveLeft();
        else if (dir == "R") tape->moveRight();

        currentState = nextState;
    };

    auto updateTable = [&]() {
        QVector<QVector<QString>> oldData;
        for (int r = 0; r < tableRules->rowCount(); ++r) {
            QVector<QString> row;
            for (int c = 0; c < tableRules->columnCount(); ++c) {
                auto* item = tableRules->item(r, c);
                row.push_back(item ? item->text() : "");
            }
            oldData.push_back(row);
        }

        tableRules->setRowCount(states.size());
        tableRules->setColumnCount(currentSymbols.size());
        tableRules->setVerticalHeaderLabels(states);
        tableRules->setHorizontalHeaderLabels(currentSymbols);

        bool cleared = false;
        for (int r = 0; r < qMin(oldData.size(), tableRules->rowCount()); ++r) {
            if (oldData[r].size() < tableRules->columnCount()) { cleared = true; break; }
            for (int c = 0; c < tableRules->columnCount(); ++c)
                tableRules->setItem(r, c, new QTableWidgetItem(oldData[r][c]));
        }
        if (cleared) {
            for (int r = 0; r < tableRules->rowCount(); ++r)
                for (int c = 0; c < tableRules->columnCount(); ++c)
                    if (!tableRules->item(r, c)) tableRules->setItem(r, c, new QTableWidgetItem(""));
        }
    };

    // === ЛОГИКА КНОПОК ===
    QObject::connect(btnAlpha, &QPushButton::clicked, [&]() {
        QString rawMain = editAlpha->text().trimmed();
        QString rawAdd  = editAdd->text().trimmed();
        currentSymbols.clear();

        for (QChar c : rawMain) {
            QString s(c);
            if (!s.isEmpty() && s != "^" && !currentSymbols.contains(s)) currentSymbols.append(s);
        }
        currentSymbols.append("^"); // ^ СТРОГО ПОСЛЕ основного алфавита
        for (QChar c : rawAdd) {
            QString s(c);
            if (!s.isEmpty() && s != "^" && !currentSymbols.contains(s)) currentSymbols.append(s);
        }

        if (currentSymbols.size() <= 1) { QMessageBox::warning(&window, "Ошибка", "Алфавит должен содержать хотя бы один рабочий символ!"); return; }
        updateTable(); tableRules->setEnabled(true); editWord->setEnabled(true); btnWord->setEnabled(true);
    });

    QObject::connect(btnAddState, &QPushButton::clicked, [&]() {
        states.append("q" + QString::number(states.size()));
        statesLabel->setText("Состояния: " + states.join(", ")); updateTable();
    });

    QObject::connect(btnRemState, &QPushButton::clicked, [&]() {
        if (states.size() <= 1) { QMessageBox::warning(&window, "Ошибка", "Должно остаться минимум одно состояние."); return; }
        states.removeLast();
        statesLabel->setText("Состояния: " + states.join(", ")); updateTable();
    });

    QObject::connect(btnWord, &QPushButton::clicked, [&]() {
        QString word = editWord->text().trimmed(); if (word.isEmpty()) return;
        for (QChar c : word) if (!currentSymbols.contains(QString(c))) { QMessageBox::warning(&window, "Ошибка ввода", QString("Символ '%1' отсутствует в заданном алфавите!").arg(c)); return; }
        tape->loadWord(word); initialWord = word; QMessageBox::information(&window, "Успех", "Строка загружена на ленту.");
    });

    QObject::connect(btnStart, &QPushButton::clicked, [&]() {
        if (initialWord.isEmpty()) { QMessageBox::warning(&window, "Ошибка", "Сначала задайте входную строку."); return; }
        if (!validateTable(true)) return;
        currentState = states.contains("q0") ? "q0" : states.first();
        isRunning = true; lockUI(true); runTimer->start();
    });

    QObject::connect(runTimer, &QTimer::timeout, [&]() { executeStep(); });

    QObject::connect(btnStep, &QPushButton::clicked, [&]() {
        if (initialWord.isEmpty()) return;
        if (!validateTable(false)) return;
        if (currentState.isEmpty()) currentState = states.contains("q0") ? "q0" : states.first();
        executeStep();
    });

    QObject::connect(btnStop, &QPushButton::clicked, [&]() {
        stopExecution("Выполнение остановлено пользователем.");
    });

    QObject::connect(btnReset, &QPushButton::clicked, [&]() {
        stopExecution("Программа сброшена.");
        if (!initialWord.isEmpty()) tape->loadWord(initialWord); else tape->clear();
        currentState = states.contains("q0") ? "q0" : states.first();
    });

    QObject::connect(btnClearTable, &QPushButton::clicked, [&]() {
        for (int r = 0; r < tableRules->rowCount(); ++r)
            for (int c = 0; c < tableRules->columnCount(); ++c) {
                auto* item = tableRules->item(r, c);
                if (item) item->setText("");
            }
        highlightCell(-1, -1);
    });

    QObject::connect(btnFaster, &QPushButton::clicked, [&]() {
        stepInterval = qMax(50, stepInterval / 2); runTimer->setInterval(stepInterval);
    });

    QObject::connect(btnSlower, &QPushButton::clicked, [&]() {
        stepInterval = qMin(3000, stepInterval * 2); runTimer->setInterval(stepInterval);
    });

    window.show(); return app.exec();
}
