#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include "tapewidget.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle("Turing Machine Tape");
    window.resize(600, 200);

    auto* tape = new TuringTape(&window);
    auto* tapeWidget = new TapeWidget(tape, &window);

    // Кнопки для ручного тестирования
    auto* btnLeft  = new QPushButton("← Left");
    auto* btnRight = new QPushButton("Right →");
    auto* btnWrite = new QPushButton("Write '1'");
    auto* btnBlank = new QPushButton("Erase");

    QObject::connect(btnLeft,  &QPushButton::clicked, tape, &TuringTape::moveLeft);
    QObject::connect(btnRight, &QPushButton::clicked, tape, &TuringTape::moveRight);
    QObject::connect(btnWrite, &QPushButton::clicked, [=]{ tape->write('1'); });
    QObject::connect(btnBlank, &QPushButton::clicked, [=]{ tape->write(tape->blankSymbol()); });

    auto* btnLayout = new QHBoxLayout;
    btnLayout->addWidget(btnLeft);
    btnLayout->addWidget(btnRight);
    btnLayout->addWidget(btnWrite);
    btnLayout->addWidget(btnBlank);
    btnLayout->addStretch();

    auto* mainLayout = new QVBoxLayout;
    mainLayout->addWidget(tapeWidget);
    mainLayout->addLayout(btnLayout);

    auto* central = new QWidget;
    central->setLayout(mainLayout);
    window.setCentralWidget(central);

    window.show();
    return app.exec();
}
