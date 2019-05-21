/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#ifndef INCLUDED_ITEM_BASE_ITEMCONTROLBLOCK_HXX
#define INCLUDED_ITEM_BASE_ITEMCONTROLBLOCK_HXX

#include <sal/types.h>
#include <item/itemdllapi.h>
#include <item/base/ItemBase.hxx>
#include <functional>
#include <memory>
#include <rtl/ustring.hxx>

///////////////////////////////////////////////////////////////////////////////

namespace Item
{
    // predefine - no need to include
    class ITEM_DLLPUBLIC ItemControlBlock
    {
    private:
        std::unique_ptr<const ItemBase>             m_aDefaultItem;
        std::function<ItemBase*()>                  m_aConstructDefaultItem;
        std::function<ItemBase*(const ItemBase&)>   m_aCloneItem;
        size_t                                      m_aHashCode;
        OUString                                    m_aName;

        // EmptyItemControlBlock: default constructor *only* for internal use
        ItemControlBlock();

    public:
        ItemControlBlock(
            std::function<ItemBase*()>aConstructDefaultItem,
            std::function<ItemBase*(const ItemBase&)>aCloneItem,
            size_t aHashCode,
            const OUString& rName);
        ~ItemControlBlock();

        const ItemBase& getDefault() const;
        bool isDefault(const ItemBase& rItem) const;

        const OUString& getName() const
        {
            return m_aName;
        }

        size_t getHashCode() const
        {
            return m_aHashCode;
        }

        // clone-op, secured by returning a std::unique_ptr to make
        // explicit the ownership you get when calling this
        std::unique_ptr<ItemBase> clone(const ItemBase&) const;

        std::unique_ptr<const ItemBase> createFromAny(const ItemBase::AnyIDArgs& rArgs);

        // static access to registered ItemControlBlocks
        static ItemControlBlock* getItemControlBlock(size_t HashCode);
        template< typename TItem > ItemControlBlock* getItemControlBlock()
        {
            return getItemControlBlock(typeid(TItem).HashCode());
        }
    };
} // end of namespace Item

///////////////////////////////////////////////////////////////////////////////

#endif // INCLUDED_ITEM_BASE_ITEMCONTROLBLOCK_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
