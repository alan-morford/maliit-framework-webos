/* * This file is part of Maliit framework *
 *
 * Copyright (C) 2026 Herman van Hazendonk <github.com@herrie.org>
 *
 * Contact: maliit-discuss@lists.maliit.org
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * and appearing in the file LICENSE.LGPL included in the packaging
 * of this file.
 */

// The weston input-method connection turns modifier indices reported by
// xkbcommon into shift counts. An index it cannot use - XKB_MOD_INVALID for a
// modifier the keymap does not define, or simply one past the width of the
// mask - used to reach "1 << index" and make the whole key event undefined.
// These are the cases that guard has to cover.

#include "mimxkbmodifiers.h"

#include <QtTest>

namespace {
// Value xkb_map_mod_get_index() returns for an undefined modifier. Spelled
// out rather than pulled from xkbcommon so this test does not need a keymap
// library to say what the guard must reject.
const uint32_t XkbModInvalid = 0xffffffffu;
}

class Ut_XkbModifiers : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testEachValidIndex();
    void testUnsetBits();
    void testInvalidIndexIsNeverSet();
    void testIndexAtAndBeyondMaskWidth_data();
    void testIndexAtAndBeyondMaskWidth();
    void testTopBitIsUsable();
    void testFullMask();
    void testEmptyMask();
    void testRealisticModifierMask();

    void testTrackerFollowsModifiers();
    void testTrackerCountsLatchedNotLocked();
    void testTrackerKeymapDropsHeldModifiers();
    void testTrackerWithoutKeymapReportsNothing();
};

void Ut_XkbModifiers::testEachValidIndex()
{
    for (uint32_t index = 0; index < Maliit::XkbModifierMaskBits; ++index) {
        const uint32_t mask = 1u << index;
        QVERIFY2(Maliit::xkbModifierIsSet(mask, index),
                 qPrintable(QString("index %1 should read as set").arg(index)));
    }
}

void Ut_XkbModifiers::testUnsetBits()
{
    // Only bit 3 is set, so every other index must read as unset.
    const uint32_t mask = 1u << 3;

    for (uint32_t index = 0; index < Maliit::XkbModifierMaskBits; ++index) {
        QCOMPARE(Maliit::xkbModifierIsSet(mask, index), index == 3);
    }
}

void Ut_XkbModifiers::testInvalidIndexIsNeverSet()
{
    // The regression this guard exists for: XKB_MOD_INVALID as a shift count.
    QVERIFY(!Maliit::xkbModifierIsSet(0u, XkbModInvalid));
    QVERIFY(!Maliit::xkbModifierIsSet(0xffffffffu, XkbModInvalid));
    QVERIFY(!Maliit::xkbModifierIsSet(1u, XkbModInvalid));
}

void Ut_XkbModifiers::testIndexAtAndBeyondMaskWidth_data()
{
    QTest::addColumn<uint32_t>("index");

    QTest::newRow("exactly the width") << Maliit::XkbModifierMaskBits;
    QTest::newRow("one past")          << (Maliit::XkbModifierMaskBits + 1);
    QTest::newRow("64")                << 64u;
    QTest::newRow("large")             << 1000000u;
    QTest::newRow("invalid")           << XkbModInvalid;
}

void Ut_XkbModifiers::testIndexAtAndBeyondMaskWidth()
{
    QFETCH(uint32_t, index);

    // All bits set: if the guard let any of these through it would still have
    // to answer, and the only safe answer for an index the mask cannot carry
    // is "not present".
    QVERIFY(!Maliit::xkbModifierIsSet(0xffffffffu, index));
}

void Ut_XkbModifiers::testTopBitIsUsable()
{
    // Index 31 is the last one that fits; it must not be swept up by the
    // range check along with the out-of-range values.
    QVERIFY(Maliit::xkbModifierIsSet(1u << 31, 31));
    QVERIFY(!Maliit::xkbModifierIsSet(~(1u << 31), 31));
}

void Ut_XkbModifiers::testFullMask()
{
    for (uint32_t index = 0; index < Maliit::XkbModifierMaskBits; ++index) {
        QVERIFY(Maliit::xkbModifierIsSet(0xffffffffu, index));
    }
}

void Ut_XkbModifiers::testEmptyMask()
{
    for (uint32_t index = 0; index < Maliit::XkbModifierMaskBits; ++index) {
        QVERIFY(!Maliit::xkbModifierIsSet(0u, index));
    }
}

void Ut_XkbModifiers::testRealisticModifierMask()
{
    // A keymap where shift is index 0 and control is index 2, with control
    // held and shift not, and no "Mod3" defined at all.
    const uint32_t shift = 0;
    const uint32_t ctrl = 2;
    const uint32_t undefinedMod = XkbModInvalid;

    const uint32_t depressed = 1u << ctrl;

    QVERIFY(!Maliit::xkbModifierIsSet(depressed, shift));
    QVERIFY(Maliit::xkbModifierIsSet(depressed, ctrl));
    QVERIFY(!Maliit::xkbModifierIsSet(depressed, undefinedMod));
}

namespace {
// The indices the default evdev keymap gives them.
const uint32_t ShiftIndex = 0;
const uint32_t LockIndex = 1;
const uint32_t ControlIndex = 2;
const uint32_t AltIndex = 3;
}

void Ut_XkbModifiers::testTrackerFollowsModifiers()
{
    Maliit::XkbModifierTracker tracker;
    tracker.keymapChanged(ShiftIndex, ControlIndex, AltIndex);

    tracker.modifiersChanged(1u << ShiftIndex, 0);
    QCOMPARE(tracker.modifiers(), Qt::KeyboardModifiers(Qt::ShiftModifier));

    tracker.modifiersChanged((1u << ControlIndex) | (1u << AltIndex), 0);
    QCOMPARE(tracker.modifiers(), Qt::ControlModifier | Qt::AltModifier);

    tracker.modifiersChanged(0, 0);
    QCOMPARE(tracker.modifiers(), Qt::KeyboardModifiers(Qt::NoModifier));
}

void Ut_XkbModifiers::testTrackerCountsLatchedNotLocked()
{
    Maliit::XkbModifierTracker tracker;
    tracker.keymapChanged(ShiftIndex, ControlIndex, AltIndex);

    // A latched Shift applies to the next key; Caps Lock is a lock, not Shift.
    tracker.modifiersChanged(0, 1u << ShiftIndex);
    QCOMPARE(tracker.modifiers(), Qt::KeyboardModifiers(Qt::ShiftModifier));

    tracker.modifiersChanged(1u << LockIndex, 0);
    QCOMPARE(tracker.modifiers(), Qt::KeyboardModifiers(Qt::NoModifier));
}

/*
 * The weston connection grabs a new keyboard for every input method context,
 * and each arrives with a keymap and a fresh xkb_state holding nothing down.
 * A Shift held when the last context ended must not outlive it in the flags
 * handed to the plugins.
 */
void Ut_XkbModifiers::testTrackerKeymapDropsHeldModifiers()
{
    Maliit::XkbModifierTracker tracker;
    tracker.keymapChanged(ShiftIndex, ControlIndex, AltIndex);
    tracker.modifiersChanged(1u << ShiftIndex, 0);
    QCOMPARE(tracker.modifiers(), Qt::KeyboardModifiers(Qt::ShiftModifier));

    tracker.keymapChanged(ShiftIndex, ControlIndex, AltIndex);
    QCOMPARE(tracker.modifiers(), Qt::KeyboardModifiers(Qt::NoModifier));

    // And the new keymap's indices are the ones used from then on.
    tracker.keymapChanged(5, 6, 7);
    tracker.modifiersChanged(1u << ShiftIndex, 0);
    QCOMPARE(tracker.modifiers(), Qt::KeyboardModifiers(Qt::NoModifier));
    tracker.modifiersChanged(1u << 5, 0);
    QCOMPARE(tracker.modifiers(), Qt::KeyboardModifiers(Qt::ShiftModifier));
}

void Ut_XkbModifiers::testTrackerWithoutKeymapReportsNothing()
{
    Maliit::XkbModifierTracker tracker;
    tracker.modifiersChanged(0xffffffffu, 0xffffffffu);
    QCOMPARE(tracker.modifiers(), Qt::KeyboardModifiers(Qt::NoModifier));

    tracker.keymapChanged(XkbModInvalid, XkbModInvalid, XkbModInvalid);
    tracker.modifiersChanged(0xffffffffu, 0xffffffffu);
    QCOMPARE(tracker.modifiers(), Qt::KeyboardModifiers(Qt::NoModifier));
}

QTEST_GUILESS_MAIN(Ut_XkbModifiers)
#include "ut_xkbmodifiers.moc"
