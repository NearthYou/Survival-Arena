#pragma once
#include "MonoBehaviour.h"
#include "BindShaderDesc.h"

enum class SkillDecalType {
    LINE,
    CIRCLE,
    CONE,
};

class SkillDecalIndicator :
    public MonoBehaviour
{
    using Super = MonoBehaviour;
public:
    SkillDecalIndicator();
    virtual ~SkillDecalIndicator();

public:
    virtual void Start() override;
    virtual void Update() override;

    void SetSkillDecal(SkillDecalType _type, float _range, float _width = 2.f);
    void SetStartPosition(const Vec3& _pos);
    void SetTargetPosition(const Vec3& _pos);
    void SetColor(const Vec4& _color);

    void ShowIndicator(bool _show);
    bool IsVisible() const { return m_isVisible; }

    void UpdateForMousePosition();

private:
    void CreateDecalObject();
    void CreateDecalMaterial();
    void CreateDecalTextures();

    void UpdateLineDecal();
    void UpdateCircleDecal();
    void UpdateConeDecal();
    void UpdateRectangleDecal();

    Vec3 GetMouseWorldPostion();
    Matrix CalculateDecalTransform(const Vec3& _center, const Vec3& _size, const Vec3& _rotation);


private:
    SkillDecalType m_decalType = SkillDecalType::LINE;
    float m_range = 10.f;
    float m_width = 2.f;

    Vec3 m_startPos = Vec3::Zero;
    Vec3 m_targetPos = Vec3::Zero;
    Vec3 m_centerPos = Vec3::Zero;
    Vec4 m_color = Vec4(0.f, 0.f, 0.5f, 1.f);

    bool m_isVisible = true;
    bool m_needsUpdate = true;

    shared_ptr<Material> m_decalMaterial;
    shared_ptr<Mesh> m_decalMesh;

    shared_ptr<Texture> m_lineTexture;
    shared_ptr<Texture> m_circleTexture;
    shared_ptr<Texture> m_coneTexture;
    shared_ptr<Texture> m_rectangleTexture;

    DecalBufferData m_decalData;
};

