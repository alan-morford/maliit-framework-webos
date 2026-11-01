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

#ifndef MIMXKBMODIFIERS_H
#define MIMXKBMODIFIERS_H

#include <cstdint>

#include <QtCore/qnamespace.h>

//! \internal
namespace Maliit
{

//! Number of modifier indices a 32-bit xkb modifier mask can represent.
const uint32_t XkbModifierMaskBits = 32;

/*! \brief Is the modifier at \a modIndex set in \a mods?
 *
 * Deliberately takes plain integers rather than xkb types so it can be
 * reasoned about - and tested - without a keymap.
 *
 * xkb_map_mod_get_index() returns XKB_MOD_INVALID (~0u) for a modifier the
 * keymap does not define, and the wl_keyboard.modifiers mask is 32 bits wide.
 * "1 << index" is undefined behaviour for any index at or beyond the width of
 * the type, so XKB_MOD_INVALID - and anything else out of range - has to be
 * rejected before the index becomes a shift count. Such a modifier simply is
 * not present, so this reports it as unset.
 */
inline bool xkbModifierIsSet(uint32_t mods, uint32_t modIndex)
{
    if (modIndex >= XkbModifierMaskBits)
        return false;

    return (mods & (1u << modIndex)) != 0;
}

/*! \brief The Qt modifiers the compositor last said are held.
 *
 * The weston connection keeps the modifier state two ways: an xkb_state,
 * which decides the character a key produces, and these Qt flags, which go to
 * the plugins with each key. Both come from wl_keyboard.modifiers. A keymap
 * replaces the xkb_state with a fresh one that holds nothing down, and it
 * arrives with every keyboard the connection grabs - one per input method
 * context. The flags used to survive it: a Shift held when one context ended
 * stayed in them into the next, so the plugins were told Shift while the
 * character came out unshifted, until the compositor happened to send
 * wl_keyboard.modifiers again.
 *
 * keymapChanged() starts the flags again with the state, and from the
 * keymap's own indices.
 */
class XkbModifierTracker
{
public:
    //! A new keymap, with the indices it gives Shift, Control and Alt.
    void keymapChanged(uint32_t shiftIndex, uint32_t controlIndex, uint32_t altIndex)
    {
        m_shiftIndex = shiftIndex;
        m_controlIndex = controlIndex;
        m_altIndex = altIndex;
        m_modifiers = Qt::NoModifier;
    }

    //! wl_keyboard.modifiers: what is held now. Locks are not modifiers here.
    void modifiersChanged(uint32_t depressed, uint32_t latched)
    {
        const uint32_t held = depressed | latched;

        m_modifiers = Qt::NoModifier;
        if (xkbModifierIsSet(held, m_controlIndex))
            m_modifiers |= Qt::ControlModifier;
        if (xkbModifierIsSet(held, m_altIndex))
            m_modifiers |= Qt::AltModifier;
        if (xkbModifierIsSet(held, m_shiftIndex))
            m_modifiers |= Qt::ShiftModifier;
    }

    Qt::KeyboardModifiers modifiers() const { return m_modifiers; }

private:
    // XKB_MOD_INVALID until a keymap says otherwise.
    uint32_t m_shiftIndex = ~0u;
    uint32_t m_controlIndex = ~0u;
    uint32_t m_altIndex = ~0u;
    Qt::KeyboardModifiers m_modifiers = Qt::NoModifier;
};

} // namespace Maliit
//! \internal_end

#endif // MIMXKBMODIFIERS_H
