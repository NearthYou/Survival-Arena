#pragma once
#include "Component.h"
#include <map>
#include <vector>

struct ImageLayer {
    shared_ptr<GameObject> gameObject;
    int layer;
    Vec2 position;
    Vec2 size;
    shared_ptr<Material> material;
    uint32 pass;

    Vec2 parentPos;

    ImageLayer() : layer(0), position(Vec2::Zero), size(Vec2::Zero), pass(0) {}
};

class ImageUI : public Component
{
public:
    ImageUI();
    virtual ~ImageUI();
    using Super = Component;

    // 이미지 레이어 추가
    void AddImageLayer(int layer, Vec2 screenPos, Vec2 size,
        shared_ptr<Material> material, uint32 pass = 0);

    // 이미지 레이어 제거
    void RemoveImageLayer(int layer);

    // 레이어 순서 변경
    void SetLayerOrder(int layer, int newOrder);

    // 레이어 위치 변경
    void SetLayerPosition(int layer, Vec2 newPosition);

    // 레이어 크기 변경
    void SetLayerSize(int layer, Vec2 newSize);

    void SetMaterial(int layer, shared_ptr<Material> material);

    void SetZPos(float zPos) { m_zPos = zPos; }

    void SetVisible(bool visible);

    // 모든 레이어 업데이트
    void UpdateLayers();
 
    // 레이어 정보 가져오기
    //const std::map<int, ImageLayer>& GetLayers() const { return m_imageLayers; }
    std::map<int, ImageLayer>& GetLayers() { return m_imageLayers; }
    Vec2 GetLayerPosition(int layer);

    virtual void Update() override;

    // 안전한 정리를 위한 함수
    void ClearAllLayers();
    virtual void OnDestroy() override;  // 추가

private:
    void CreateImageGameObject(ImageLayer& layer);
    void UpdateImageGameObject(ImageLayer& layer);
    void SortLayersByOrder();

private:
    std::map<int, ImageLayer> m_imageLayers;  // layer -> ImageLayer
    std::vector<int> m_sortedLayers;          // 정렬된 레이어 순서
    bool m_needsSort;
    bool m_isDestroying;  // 소멸 중인지 확인하는 플래그
    bool m_visible = true;

    float m_zPos = 0.4f;
private:
    Vec2 m_localPosition;  // 부모(UIPanel) 기준 로컬 위치
    Vec2 m_size;           // 버튼 크기

public:
    void UpdatePosition(const Vec2& parentWorldPos);  // 부모 위치 기준으로 업데이트
    void SetLocalPosition(const Vec2& localPos) { m_localPosition = localPos; }
    const Vec2& GetLocalPosition() const { return m_localPosition; }
    void UpdatePickingRect(const Vec2& screenPos);
};