#pragma once
#include "MonoBehaviour.h"

class Item;

class ItemBox :
    public MonoBehaviour
{
    using Super = MonoBehaviour;

public:
    ItemBox();
    virtual ~ItemBox();


    virtual void Start() override;
    virtual void Update() override;
    virtual void LateUpdate() override;

public:
    shared_ptr<Item> InsertItem(int _index, shared_ptr<Item> _item);
    shared_ptr<Item> DeleteItem(int _index);

    bool PushItem(shared_ptr<Item> _item);

    array<shared_ptr<Item>, 8>& GetBoxInventory() {
        return m_boxInventory;
    }

private:
    array<shared_ptr<Item>, 8> m_boxInventory;
    //shared_ptr<AABBBoxCollider> m_collider;
};

