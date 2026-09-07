#include "pch.h"
#include "Item.h"

Item::Item()
{
}

Item::~Item()
{
    if (m_itemImage != nullptr)
        m_itemImage.reset();
}