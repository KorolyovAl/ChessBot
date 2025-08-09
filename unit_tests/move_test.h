#pragma once

#include <QObject>
#include <QtTest>

class MoveTest : public QObject {
    Q_OBJECT

private slots:
    void DefaultMoveShouldHaveNoneFields();
    void MoveConstructorShouldStoreValues();
    void FlagShouldBeSetAndRead();
};
