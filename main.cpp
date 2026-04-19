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
    QLabel* lblEmpty = new QLabel("^");
    lblEmpty->setFixedWidth(30); lblEmpty->setAlignment(Qt::AlignCenter);
    lblEmpty->setStyleSheet("background: #f0f0f0; border: 1px solid #aaa; border-radius: 4px; font-weight: bold;");
    alphaLayout->addWidget(lblEmpty);
    alphaLayout->addWidget(new QLabel("Доп. символы:"));
    alphaLayout->addWidget(editAdd);
    alphaLayout->addWidget(btnAlpha);

    // === СОСТОЯНИЯ (+ и -) ===
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
    auto* btnStep  = new QPushButton("▶ Шаг");
    auto* btnStart = new QPushButton("▶▶ Запустить");
    auto* btnStop  = new QPushButton("⏹ Остановить");
    auto* btnReset = new QPushButton("🔄 Сброс");
    auto* btnFaster= new QPushButton("⏩ Быстрее");
    auto* btnSlower= new QPushButton("⏪ Медленнее");

    auto* controlLayout = new QHBoxLayout;
    controlLayout->addWidget(btnStep); controlLayout->addWidget(btnStart);
    controlLayout->addWidget(btnStop); controlLayout->addWidget(btnReset);
    controlLayout->addWidget(btnFaster); controlLayout->addWidget(btnSlower);
    controlLayout->addStretch();

    // === КОМПОНОВКА ===
    auto* mainLayout = new QVBoxLayout;
    mainLayout->addLayout(alphaLayout); mainLayout->addLayout(statesLayout);
    mainLayout->addWidget(tableRules, 2);
    mainLayout->addWidget(new QLabel("Формат ячейки: Символ;Направление;Состояние (допустимы запятые) | ;; = стоп | ! = экстренный стоп"));
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
    QString lastDir = "S";
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
    };

    auto highlightState = [&](const QString& state) {
        tableRules->clearSelection();
        for (int r = 0; r < tableRules->rowCount(); ++r) {
            for (int c = 0; c < tableRules->columnCount(); ++c) {
                auto* item = tableRules->item(r, c);
                if (item) item->setData(Qt::BackgroundRole, QVariant());
            }
            if (tableRules->verticalHeaderItem(r)->text() == state) {
                for (int c = 0; c < tableRules->columnCount(); ++c) {
                    auto* item = tableRules->item(r, c);
                    if (item) item->setData(Qt::BackgroundRole, QColor(255, 250, 205));
                }
            }
        }
    };

    auto stopExecution = [&](const QString& reason) {
        isRunning = false;
        runTimer->stop();
        lockUI(false);
        QMessageBox::information(&window, "Остановка", reason);
    };

    auto validateTable = [&]() -> bool {
        bool hasHalt = false;
        for (const QString& s : states) if (s.toLower().contains("halt") || s.toLower().contains("stop")) hasHalt = true;
        if (!hasHalt) {
            QMessageBox::warning(&window, "Ошибка валидации",
                                 "В списке состояний отсутствует терминальное состояние (например, qHalt или Stop).\nЗапуск запрещён.");
            return false;
        }

        QRegularExpression rulePattern(R"(^[^;]*;[LRS]?;[^;]*$)");
        for (int r = 0; r < tableRules->rowCount(); ++r) {
            for (int c = 0; c < tableRules->columnCount(); ++c) {
                auto* item = tableRules->item(r, c);
                if (!item) continue;
                QString txt = item->text().trimmed();
                if (txt.isEmpty() || txt == ";;" || txt == ",,") continue;

                QString normalized = txt.replace(',', ';');
                if (!rulePattern.match(normalized).hasMatch()) {
                    QMessageBox::warning(&window, "Ошибка формата ячейки",
                                         QString("Ячейка [%1, %2] содержит недопустимый формат.\nОжидается: Символ;Направление(L/R/S);Состояние").arg(r+1).arg(c+1));
                    return false;
                }
            }
        }
        return true;
    };

    auto executeStep = [&]() {
        if (tape->read() == '!') {
            stopExecution("Головка прочитала символ '!'. Аварийная остановка."); return;
        }

        int stateRow = states.indexOf(currentState);
        int symCol = currentSymbols.indexOf(QString(tape->read()));

        if (stateRow == -1 || symCol == -1) {
            stopExecution("Нет правила для текущего состояния и символа."); return;
        }

        auto* cell = tableRules->item(stateRow, symCol);
        if (!cell || cell->text().isEmpty()) {
            stopExecution("Ячейка перехода пуста. Машина остановлена."); return;
        }

        QString cellText = cell->text().trimmed();
        if (cellText == ";;" || cellText == ",,") {
            stopExecution("Встречена команда остановки ';;' или ',,'."); return;
        }

        if (cellText.contains('!')) {
            stopExecution("В правиле таблицы обнаружен символ '!'. Аварийная остановка."); return;
        }

        QString normalized = cellText.replace(',', ';');
        QStringList parts = normalized.split(';');
        if (parts.size() != 3) { stopExecution("Ошибка парсинга ячейки."); return; }

        QChar currentSym = tape->read();
        QChar writeSym   = parts[0].isEmpty() ? currentSym : parts[0][0];
        QString dir      = parts[1].trimmed().isEmpty() ? lastDir : parts[1].trimmed().toUpper();
        QString nextState= parts[2].isEmpty()  ? currentState : parts[2];

        tape->write(writeSym);
        if (dir == "L") { tape->moveLeft(); lastDir = "L"; }
        else if (dir == "R") { tape->moveRight(); lastDir = "R"; }
        else { lastDir = "S"; }

        currentState = nextState;
        highlightState(currentState);

        if (currentState.toLower().contains("halt") || currentState.toLower().contains("stop")) {
            stopExecution("Достигнуто терминальное состояние. Успешная остановка.");
        }
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
        for (QChar c : rawMain) { QString s(c); if(!s.isEmpty() && !currentSymbols.contains(s)) currentSymbols.append(s); }
        if (!currentSymbols.contains("^")) currentSymbols.append("^");
        for (QChar c : rawAdd) { QString s(c); if(!s.isEmpty() && !currentSymbols.contains(s)) currentSymbols.append(s); }

        if (currentSymbols.size() <= 1) { QMessageBox::warning(&window, "Ошибка", "Алфавит должен содержать хотя бы один рабочий символ!"); return; }
        updateTable(); tableRules->setEnabled(true); editWord->setEnabled(true); btnWord->setEnabled(true);
    });

    QObject::connect(btnAddState, &QPushButton::clicked, [&]() {
        states.append("q" + QString::number(states.size()));
        statesLabel->setText("Состояния: " + states.join(", ")); updateTable();
    });

    // ✅ ИСПРАВЛЕНО: - теперь мгновенно удаляет последнее состояние
    QObject::connect(btnRemState, &QPushButton::clicked, [&]() {
        if (states.size() <= 1) {
            QMessageBox::warning(&window, "Ошибка", "Должно остаться минимум одно состояние.");
            return;
        }
        states.removeLast(); // Удаляем qN
        statesLabel->setText("Состояния: " + states.join(", "));
        updateTable();
    });

    QObject::connect(btnWord, &QPushButton::clicked, [&]() {
        QString word = editWord->text().trimmed(); if (word.isEmpty()) return;
        for (QChar c : word) if (!currentSymbols.contains(QString(c))) { QMessageBox::warning(&window, "Ошибка ввода", QString("Символ '%1' отсутствует в заданном алфавите!").arg(c)); return; }
        tape->loadWord(word); initialWord = word; QMessageBox::information(&window, "Успех", "Строка загружена на ленту.");
    });

    QObject::connect(btnStart, &QPushButton::clicked, [&]() {
        if (initialWord.isEmpty()) { QMessageBox::warning(&window, "Ошибка", "Сначала задайте входную строку."); return; }
        if (!validateTable()) return;
        currentState = states.contains("q0") ? "q0" : states.first();
        lastDir = "S";
        isRunning = true; lockUI(true); highlightState(currentState); runTimer->start();
    });

    QObject::connect(runTimer, &QTimer::timeout, [&]() { executeStep(); });

    QObject::connect(btnStep, &QPushButton::clicked, [&]() {
        if (initialWord.isEmpty()) return;
        if (!validateTable()) return;
        if (currentState.isEmpty()) currentState = states.contains("q0") ? "q0" : states.first();
        executeStep(); highlightState(currentState);
    });

    QObject::connect(btnStop, &QPushButton::clicked, [&]() {
        stopExecution("Выполнение остановлено пользователем.");
    });

    QObject::connect(btnReset, &QPushButton::clicked, [&]() {
        stopExecution("Программа сброшена.");
        if (!initialWord.isEmpty()) tape->loadWord(initialWord); else tape->clear();
        currentState = states.contains("q0") ? "q0" : states.first();
        lastDir = "S";
        highlightState("");
    });

    QObject::connect(btnFaster, &QPushButton::clicked, [&]() {
        stepInterval = qMax(50, stepInterval / 2); runTimer->setInterval(stepInterval);
    });

    QObject::connect(btnSlower, &QPushButton::clicked, [&]() {
        stepInterval = qMin(3000, stepInterval * 2); runTimer->setInterval(stepInterval);
    });

    window.show(); return app.exec();
}
