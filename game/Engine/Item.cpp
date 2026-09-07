#include "pch.h"
#include "Item.h"

Item::Item()
{
}

Item::~Item()
{
    if (m_itemMaterial != nullptr)
        m_itemMaterial.reset();
}