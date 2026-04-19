#pragma once
#include <QObject>
#include <QHash>
#include <QChar>

class TuringTape : public QObject {
    Q_OBJECT
public:
    explicit TuringTape(QObject* parent = nullptr) : QObject(parent) {}

    QChar read() const { return symbolAt(headPos); }
    void write(QChar sym) {
        if (sym == blank) memory.remove(headPos);
        else memory[headPos] = sym;
        emit changed();
    }

    void moveLeft()  { --headPos; emit changed(); }
    void moveRight() { ++headPos; emit changed(); }

    QChar symbolAt(int index) const { return memory.value(index, blank); }
    int headPosition() const { return headPos; }
    QChar blankSymbol() const { return blank; }

    void clear() {
        memory.clear(); headPos = 0; emit changed();
    }
    void loadWord(const QString& word) {
        clear();
        for (int i = 0; i < word.length(); ++i) {
            if (word[i] != blank) memory[i] = word[i];
        }
        headPos = 0;
        emit changed();
    }
signals:
    void changed();

private:
    QHash<int, QChar> memory;
    int headPos = 0;
    QChar blank = '^';
};
