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
#include <QtMath>
#include "tapewidget.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QMainWindow window;
    window.setWindowTitle("Машина Тьюринга");
    window.resize(900, 700);

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
    lblEmpty->setFixedWidth(30);
    lblEmpty->setAlignment(Qt::AlignCenter);
    lblEmpty->setStyleSheet("background: #f0f0f0; border: 1px solid #aaa; border-radius: 4px; font-weight: bold;");
    lblEmpty->setToolTip("Символ пустой ячейки. Всегда присутствует в алфавите.");
    alphaLayout->addWidget(lblEmpty);

    alphaLayout->addWidget(new QLabel("Доп. символы:"));
    alphaLayout->addWidget(editAdd);
    alphaLayout->addWidget(btnAlpha);

    // === СОСТОЯНИЯ ===
    QStringList states = {"q0"};
    auto* statesLabel = new QLabel("Состояния: q0");
    auto* btnAddState = new QPushButton("+");
    btnAddState->setFixedWidth(30);

    auto* statesLayout = new QHBoxLayout;
    statesLayout->addWidget(btnAddState);
    statesLayout->addWidget(statesLabel);
    statesLayout->addStretch();

    // === ТАБЛИЦА ===
    auto* tableRules = new QTableWidget(0, 0);
    tableRules->setEditTriggers(QAbstractItemView::DoubleClicked);
    tableRules->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tableRules->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    tableRules->setEnabled(false);

    // === ВХОДНОЕ СЛОВО ===
    auto* editWord = new QLineEdit;
    auto* btnWord  = new QPushButton("Задать строку");
    editWord->setEnabled(false);
    btnWord->setEnabled(false);

    auto* wordLayout = new QHBoxLayout;
    wordLayout->addWidget(new QLabel("Входное слово:"));
    wordLayout->addWidget(editWord);
    wordLayout->addWidget(btnWord);
    wordLayout->addStretch();

    // === УПРАВЛЕНИЕ ===
    auto* btnStep  = new QPushButton("Шаг");
    auto* btnStart = new QPushButton("Старт");
    auto* btnStop  = new QPushButton("Стоп");
    auto* btnReset = new QPushButton("Сброс");

    auto* controlLayout = new QHBoxLayout;
    controlLayout->addWidget(btnStep);
    controlLayout->addWidget(btnStart);
    controlLayout->addWidget(btnStop);
    controlLayout->addWidget(btnReset);
    controlLayout->addStretch();

    // === КОМПОНОВКА ===
    auto* mainLayout = new QVBoxLayout;
    mainLayout->addLayout(alphaLayout);
    mainLayout->addLayout(statesLayout);
    mainLayout->addWidget(tableRules, 2);
    mainLayout->addWidget(new QLabel("Формат ячейки: Запись;Направление(L/R/S);Состояние"));
    mainLayout->addLayout(wordLayout);
    mainLayout->addLayout(controlLayout);
    mainLayout->addWidget(tapeWidget, 3);

    auto* central = new QWidget;
    central->setLayout(mainLayout);
    window.setCentralWidget(central);

    // === ЛОГИКА ===
    QStringList currentSymbols;

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

        for (int r = 0; r < qMin(oldData.size(), tableRules->rowCount()); ++r) {
            for (int c = 0; c < qMin(oldData.first().size(), tableRules->columnCount()); ++c) {
                tableRules->setItem(r, c, new QTableWidgetItem(oldData[r][c]));
            }
        }
    };

    QObject::connect(btnAlpha, &QPushButton::clicked, [&]() {
        QString rawMain = editAlpha->text().trimmed();
        QString rawAdd  = editAdd->text().trimmed();
        currentSymbols.clear();

        // 1️⃣ Сначала основной алфавит
        for (QChar c : rawMain) {
            QString s(c);
            if (!s.isEmpty() && !currentSymbols.contains(s)) currentSymbols.append(s);
        }
        // 2️⃣ Затем пустота ^ (гарантированно справа от основного алфавита)
        if (!currentSymbols.contains("^")) currentSymbols.append("^");
        // 3️⃣ Затем дополнительные символы
        for (QChar c : rawAdd) {
            QString s(c);
            if (!s.isEmpty() && !currentSymbols.contains(s)) currentSymbols.append(s);
        }

        if (currentSymbols.size() <= 1) {
            QMessageBox::warning(&window, "Ошибка", "Алфавит должен содержать хотя бы один рабочий символ!");
            return;
        }

        updateTable();
        tableRules->setEnabled(true);
        editWord->setEnabled(true);
        btnWord->setEnabled(true);
    });

    QObject::connect(btnAddState, &QPushButton::clicked, [&]() {
        states.append("q" + QString::number(states.size()));
        statesLabel->setText("Состояния: " + states.join(", "));
        updateTable();
    });

    QObject::connect(btnWord, &QPushButton::clicked, [&]() {
        QString word = editWord->text().trimmed();
        if (word.isEmpty()) return;

        for (QChar c : word) {
            if (!currentSymbols.contains(QString(c))) {
                QMessageBox::warning(&window, "Ошибка",
                                     QString("Символ '%1' не входит в заданный алфавит!").arg(c));
                return;
            }
        }
        tape->loadWord(word);
        QMessageBox::information(&window, "Успех", "Слово загружено на ленту.");
    });

    QObject::connect(btnStep,  &QPushButton::clicked, [&]{
        // TODO: Выполнить один шаг машины Тьюринга
    });
    QObject::connect(btnStart, &QPushButton::clicked, [&]{
        // TODO: Запустить автовыполнение по таймеру
    });
    QObject::connect(btnStop,  &QPushButton::clicked, [&]{
        // TODO: Остановить таймер автовыполнения
    });
    QObject::connect(btnReset, &QPushButton::clicked, [&]{
        tape->clear();
        editWord->clear();
        // TODO: Сбросить текущее состояние машины на q0
    });

    window.show();
    return app.exec();
}
