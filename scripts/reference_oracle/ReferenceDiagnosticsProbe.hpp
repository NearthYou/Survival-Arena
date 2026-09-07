#pragma once
#include "ReferenceEngineCounters.hpp"
#include "ReferenceProbeInput.hpp"
#include "ReferenceFrameCapture.hpp"
#include "Player.h"
#include "InventoryManager.h"
#include "NavMesh.h"
#include "QuadTree.h"
#include "SphereCollider.h"
#include "ModelRenderer.h"
#include "Model.h"
#include "ModelMesh.h"
#include "Material.h"
#include "IndexBuffer.h"
#include "RenderManager.h"
#include "SceneObjectManager.h"
#include "Camera.h"
#include "Viewport.h"
#include <chrono>
#include <fstream>
#include <set>
#include <cmath>

class ReferenceDiagnostics
{
    using Clock = std::chrono::steady_clock;
    struct RenderCase { unsigned count; bool individual; };
    const std::array<RenderCase, 9> m_renderCases{{
        {0,false}, {128,false}, {128,true}, {512,true}, {512,false},
        {1024,false}, {1024,true}, {128,true}, {128,false}}};
public:
    void Tick()
    {
        if (!std::dynamic_pointer_cast<LumiaIsland>(SCENE->GetCurScene()) || m_finished) return;
        auto& input = ReferenceProbeInput();
        input.keys.fill(false);
        const auto now = Clock::now();
        if (!m_pendingCapture.empty() && now >= m_captureAt)
        {
            ReferenceQueueFrame(m_pendingCapture.c_str());
            m_pendingCapture.clear();
        }
        if (!m_player)
        {
            m_player = InventoryManager::GetInstance()->GetPlayer();
            if (!m_player || !m_player->GetModelAnimator()) { m_player.reset(); return; }
            for (const auto& object : CURSCENE->GetObjects())
                if (auto nav = object->GetComponent<NavMesh>()) { m_nav = nav; break; }
            if (!m_nav) throw std::runtime_error("Diagnostics need the loaded game NavMesh");
            m_stageAt = m_lastTick = now;
            return;
        }
        const double frameMs = Milliseconds(now - m_lastTick);
        m_lastTick = now;
        const double age = std::chrono::duration<double>(now - m_stageAt).count();
        if (m_stage == -1)
        {
            if (age < 2) return;
            MeasurePaths();
            m_stage = 0;
            m_stageAt = Clock::now();
            QueueMarker(L"diagnostics-path.bmp");
        }
        else if (m_stage == 0)
        {
            DrawPath();
            Panel("PATHFINDING", "A* on the loaded game NavMesh", "Green line: computed route; right-click command follows it");
            if (!m_moveRequested && age > 0.5 && m_route.size() > 1)
            {
                const auto point = Project(m_route.back());
                input.mouse = POINT{static_cast<LONG>(point.x), static_cast<LONG>(point.y)};
                input.keys[VK_RBUTTON] = true;
                m_moveRequested = true;
            }
            if (age > 10)
            {
                m_pathMoved = Vec3::Distance(m_walkStart, m_player->GetTransform()->GetPosition());
                PrepareRenderObjects();
                m_stage = 1;
                BeginRenderCase(0);
            }
        }
        else if (m_stage == 1)
        {
            const auto current = m_renderCases[m_renderIndex];
            const auto counts = ReferenceDraws();
            const auto description = std::to_string(current.count) + " copies / "
                + (current.individual ? "individual draws" : "instanced draws");
            const auto live = "Draw calls: " + std::to_string(counts.calls) + " / frame interval: " + std::to_string(frameMs).substr(0,5) + " ms";
            Panel("RENDER LOAD", description, live);
            if (age >= 2 && frameMs > 0)
                m_renderCsv << m_renderIndex << ',' << current.count << ',' << current.individual << ','
                    << frameMs << ',' << counts.calls << ',' << counts.instances << ',' << counts.elements << ','
                    << GRAPHICS->GetLastPresentResult() << '\n';
            if (age >= 8)
            {
                if (++m_renderIndex < m_renderCases.size()) BeginRenderCase(m_renderIndex);
                else
                {
                    for (auto& object : m_renderObjects) object->SetActive(false);
                    GET_SINGLE(RenderManager)->Clear();
                    m_renderCsv.flush();
                    m_stage = 2;
                    MeasureQuadTree(128);
                    m_stageAt = Clock::now();
                }
            }
        }
        else if (m_stage == 2)
        {
            DrawTree();
            Panel("QUADTREE", std::to_string(m_quadObjects.size()) + (m_quadsValid ? " sphere colliders / overlap results match all pairs" : " sphere colliders / RESULT MISMATCH"),
                "Candidate tests: " + std::to_string(m_tree->GetStats().lastCollisionPairs));
            if (age > 5)
            {
                if (m_quadObjects.size() == 128) MeasureQuadTree(512);
                else if (m_quadObjects.size() == 512) MeasureQuadTree(1024);
                else { Finish(); return; }
                m_stageAt = Clock::now();
            }
        }
    }

private:
    void QueueMarker(const std::wstring& name)
    {
        m_pendingCapture = name;
        m_captureAt = Clock::now() + std::chrono::seconds(1);
    }

    template<class Duration> static double Milliseconds(Duration value)
    { return std::chrono::duration<double, std::milli>(value).count(); }

    ImVec2 Project(const Vec3& position)
    {
        const auto camera = CURSCENE->GetMainCamera()->GetCamera();
        const auto point = GRAPHICS->GetViewport().Project(position, Matrix::Identity,
            camera->GetViewMatrix(), camera->GetProjectionMatrix());
        return ImVec2(point.x, point.y);
    }

    void Panel(const std::string& title, const std::string& description, const std::string& detail)
    {
        auto draw = ImGui::GetForegroundDrawList();
        draw->AddRectFilled(ImVec2(26,24), ImVec2(920,164), IM_COL32(12,22,28,238), 8);
        draw->AddText(ImGui::GetFont(), 28, ImVec2(46,38), IM_COL32(225,180,112,255), title.c_str());
        draw->AddText(ImGui::GetFont(), 21, ImVec2(46,79), IM_COL32(240,244,239,255), description.c_str());
        draw->AddText(ImGui::GetFont(), 18, ImVec2(46,113), IM_COL32(155,186,189,255), detail.c_str());
#if defined(DXA_GAME_RECORD_VIDEO)
        draw->AddText(ImGui::GetFont(), 15, ImVec2(46,141), IM_COL32(155,186,189,255), "RECORDING - performance results come from the separate capture-free run");
#endif
    }

    bool ValidRoute(const std::vector<Vec3>& route)
    {
        if (route.size() < 2) return false;
        for (size_t i = 1; i < route.size(); ++i)
        {
            const auto distance = Vec3::Distance(route[i-1], route[i]);
            const int steps = static_cast<int>(std::ceil(distance / 0.25f));
            for (int j = 0; j <= steps; ++j)
            {
                const auto point = Vec3::Lerp(route[i-1], route[i], steps ? static_cast<float>(j)/steps : 0.f);
                if (!m_nav->IsOnNavMesh(point,0.1f)) return false;
            }
        }
        return true;
    }

    void MeasurePaths()
    {
        std::ofstream output("diagnostics-paths.csv");
        output << "query,repeat,elapsed_us,expanded_nodes,points,length,direct_distance,valid\n";
        const auto start = m_nav->GetNearestPointOnNavMesh(m_player->GetTransform()->GetPosition());
        const std::array<Vec3,6> destinations{{start+Vec3(12,0,10), Vec3(20,18,15.22f), Vec3(45.587f,18,49.758f),
            Vec3(48.587f,18,48.268f), Vec3(72,18,13.228f), Vec3(116.722f,18,108.22f)}};
        m_walkStart = m_player->GetTransform()->GetPosition();
        for (const auto& object:CURSCENE->GetObjects())
            if (object->GetType()==OBJECTTYPE::MONSTER) object->SetActive(false);
        for (size_t query = 0; query < destinations.size(); ++query)
        {
            const auto end = m_nav->GetNearestPointOnNavMesh(destinations[query]);
            std::vector<Vec3> route;
            m_nav->FindPath(start,end,route);
            const bool valid = ValidRoute(route);
            m_pathsValid = m_pathsValid && valid;
            float length = 0;
            for (size_t i=1;i<route.size();++i) length += Vec3::Distance(route[i-1],route[i]);
            if (query == 0) m_route = route;
            if (valid && route.size() > 2 && length < 35) m_route = route;
            const int repeats =
#if defined(DXA_GAME_RECORD_VIDEO)
                1;
#else
                100;
#endif
            for (int repeat=-5;repeat<repeats;++repeat)
            {
                const auto begin=Clock::now();
                m_nav->FindPath(start,end,route);
                const auto us=std::chrono::duration<double,std::micro>(Clock::now()-begin).count();
                if(repeat>=0) output << query << ',' << repeat << ',' << us << ',' << m_nav->GetLastExpandedNodes()
                    << ',' << route.size() << ',' << length << ',' << Vec3::Distance(start,end) << ',' << valid << '\n';
            }
        }
        // Choose a visible detour for the movement demonstration; measured queries stay fixed.
        float bestLength = 1000;
        for (float x : {12.f,18.f,24.f})
            for (float z : {8.f,14.f,20.f})
            {
                std::vector<Vec3> route;
                const auto end=m_nav->GetNearestPointOnNavMesh(start+Vec3(x,0,z));
                m_nav->FindPath(start,end,route);
                float length=0;
                for(size_t i=1;i<route.size();++i) length+=Vec3::Distance(route[i-1],route[i]);
                if(length<bestLength && length<35 && length>Vec3::Distance(start,end)*1.05f && ValidRoute(route))
                { bestLength=length; m_route=route; }
            }
    }

    void DrawPath()
    {
        auto draw=ImGui::GetForegroundDrawList();
        for(size_t i=1;i<m_route.size();++i)
        {
            const auto a=Project(m_route[i-1]+Vec3(0,0.3f,0));
            const auto b=Project(m_route[i]+Vec3(0,0.3f,0));
            draw->AddLine(a,b,IM_COL32(75,244,145,245),4);
            draw->AddCircleFilled(b,5,IM_COL32(240,230,151,255));
        }
    }

    void PrepareRenderObjects()
    {
        shared_ptr<GameObject> prototype;
        for(const auto& object:CURSCENE->GetObjects())
        {
            const auto renderer=object->GetModelRenderer();
            if(!renderer || !renderer->GetModel() || object->GetType()!=OBJECTTYPE::MAP || !object->GetActive()
                || object->GetName().rfind(L"Cemetery_OBJ_",0)!=0 || object->GetNavMesh()) continue;
            auto model=renderer->GetModel();
            bool opaque=true;
            for(const auto& material:model->GetMaterials())
                opaque=opaque && !material->IsTransparent() && material->GetRenderingMode()==RenderingMode::Deferred;
            if(!opaque) continue;
            unsigned indices=0;
            for(const auto& mesh:model->GetMeshes()) indices+=mesh->m_indexBuffer->GetCount();
            if(indices>=300 && indices<=6000 && model->GetBoneCount()<=200 &&
                (!prototype || object->GetName()<prototype->GetName()))
            { prototype=object; m_modelIndices=indices; }
        }
        if(!prototype) throw std::runtime_error("No bounded static game model for rendering benchmark");
        const auto renderer=prototype->GetModelRenderer();
        m_modelPass=renderer->GetDiagnosticPass();
        std::ofstream details("diagnostics-fixture.txt");
        details << "model_indices=" << m_modelIndices << "\nmodel_pass=" << static_cast<unsigned>(m_modelPass)
            << "\nmodel_meshes=" << renderer->GetModel()->GetMeshCount() << "\nopaque=true\nprototype=";
        for(auto c:prototype->GetName()) details << static_cast<char>(c);
        details << '\n';
        const auto center=m_player->GetTransform()->GetPosition();
        // Only this diagnostic scene freezes actors and adds render-only copies.
        for(const auto& object:CURSCENE->GetObjects())
            if(object->GetType()==OBJECTTYPE::PLAYER || object->GetType()==OBJECTTYPE::MONSTER) object->SetActive(false);
        for(unsigned i=0;i<1024;++i)
        {
            auto object=make_shared<GameObject>();
            object->SetName(L"RenderLoadCopy");
            object->GetTransform()->SetScale(prototype->GetTransform()->GetScale());
            object->GetTransform()->SetRotation(prototype->GetTransform()->GetRotation());
            object->GetTransform()->SetPosition(center+Vec3((static_cast<float>(i%32)-15.5f)*0.5f,0,(static_cast<float>(i/32)-15.5f)*0.5f));
            object->AddComponent(make_shared<ModelRenderer>(renderer->GetDiagnosticShader()));
            object->GetModelRenderer()->SetModel(renderer->GetModel());
            object->GetModelRenderer()->SetPass(m_modelPass);
            object->SetActive(false);
            CURSCENE->Add(object);
            object->Start();
            m_renderObjects.push_back(object);
        }
        m_renderCsv.open("diagnostics-render.csv");
        m_renderCsv << "phase,objects,individual,frame_ms,draw_calls,submitted_instances,submitted_elements,previous_present_result\n";
    }

    void BeginRenderCase(size_t index)
    {
        GET_SINGLE(RenderManager)->Clear();
        const auto current=m_renderCases[index];
        for(size_t i=0;i<m_renderObjects.size();++i)
        {
            m_renderObjects[i]->SetActive(i<current.count);
            m_renderObjects[i]->GetModelRenderer()->SetDiagnosticSingleDraw(current.individual);
        }
        m_stageAt=Clock::now();
        const auto marker=L"diagnostics-render-"+std::to_wstring(index)+L"-"+std::to_wstring(current.count)+(current.individual?L"-individual.bmp":L"-instanced.bmp");
        QueueMarker(marker);
    }

    static ULONG64 PairID(shared_ptr<BaseCollider> a, shared_ptr<BaseCollider> b)
    {
        COLLIDER_ID id;
        id.left_id=(std::min)(a->GetID(),b->GetID());
        id.right_id=(std::max)(a->GetID(),b->GetID());
        return id.ID;
    }

    void MeasureQuadTree(unsigned count)
    {
        for(auto& object:m_quadObjects) object->SetActive(false);
        m_quadObjects.clear();
        const auto center=m_player->GetTransform()->GetPosition();
        const auto columns=static_cast<unsigned>(std::ceil(std::sqrt(static_cast<double>(count))));
        const float spacing=16.f/columns;
        std::vector<shared_ptr<BaseCollider>> colliders;
        for(unsigned i=0;i<count;++i)
        {
            auto object=make_shared<GameObject>();
            object->SetName(L"QuadTreeSample");
            const float x=(static_cast<float>(i%columns)-columns/2.f)*spacing-(i%8==1?spacing*0.6f:0.f);
            const float z=(static_cast<float>(i/columns)-columns/2.f)*spacing;
            object->GetTransform()->SetPosition(center+Vec3(x,0,z));
            object->GetTransform()->Update();
            auto collider=make_shared<SphereCollider>();
            object->AddComponent(collider);
            collider->SetRadius(spacing*0.4f);
            collider->Update();
            collider->SetVisible(false);
            colliders.push_back(collider);
            m_quadObjects.push_back(object);
        }
        std::ofstream output("diagnostics-quadtree.csv",std::ios::app);
        if(count==128) output << "objects,repeat,all_pair_tests,tree_pair_tests,all_pairs_us,tree_build_us,tree_collision_us,hits,results_equal,nodes\n";
        const int repeats=
#if defined(DXA_GAME_RECORD_VIDEO)
            1;
#else
            15;
#endif
        for(int repeat=-2;repeat<repeats;++repeat)
        {
            std::set<ULONG64> naive;
            auto begin=Clock::now();
            for(size_t i=0;i<colliders.size();++i)
                for(size_t j=i+1;j<colliders.size();++j)
                    if(colliders[i]->Intersects(colliders[j])) naive.insert(PairID(colliders[i],colliders[j]));
            const double naiveUs=std::chrono::duration<double,std::micro>(Clock::now()-begin).count();
            begin=Clock::now();
            m_tree=make_unique<QuadTree>(GRAPHICS->GetViewport().GetWidth(),GRAPHICS->GetViewport().GetHeight());
            for(auto& object:m_quadObjects) m_tree->Insert(object);
            m_tree->Build();
            const double buildUs=std::chrono::duration<double,std::micro>(Clock::now()-begin).count();
            unordered_map<ULONG64,bool> collisions;
            begin=Clock::now();
            m_tree->CheckCollisionsInTree(CURSCENE->GetMainCamera()->GetCamera(),collisions);
            const double treeUs=std::chrono::duration<double,std::micro>(Clock::now()-begin).count();
            std::set<ULONG64> accelerated;
            for(const auto& collision:collisions) if(collision.second) accelerated.insert(collision.first);
            const bool equal=naive==accelerated && !naive.empty() && m_tree->GetInsertedObject().size()==count;
            m_quadsValid=m_quadsValid && equal;
            if(repeat>=0) output << count << ',' << repeat << ',' << (static_cast<uint64_t>(count)*(count-1)/2)
                << ',' << m_tree->GetStats().lastCollisionPairs << ',' << naiveUs << ',' << buildUs << ',' << treeUs
                << ',' << naive.size() << ',' << equal << ',' << m_tree->GetStats().totalNodes << '\n';
        }
        const auto marker=L"diagnostics-quadtree-"+std::to_wstring(count)+L".bmp";
        QueueMarker(marker);
    }

    void DrawTree()
    {
        std::vector<RECT> bounds;
        std::vector<int> depths;
        m_tree->GetNodeBounds(bounds,depths);
        auto draw=ImGui::GetForegroundDrawList();
        for(size_t i=0;i<bounds.size();++i)
            draw->AddRect(ImVec2(static_cast<float>(bounds[i].left),static_cast<float>(bounds[i].top)),
                ImVec2(static_cast<float>(bounds[i].right),static_cast<float>(bounds[i].bottom)),IM_COL32(58,210,200,140),0,0,1);
        for(const auto& object:m_quadObjects)
            draw->AddCircleFilled(Project(object->GetTransform()->GetPosition()),3,IM_COL32(248,191,109,240));
    }

    void Finish()
    {
        m_finished=true;
        std::ofstream result("diagnostics.json");
        const bool passed=m_pathsValid && m_quadsValid && m_pathMoved>1;
        result << "{\"passed\":" << (passed?"true":"false") << ",\"paths_valid\":" << m_pathsValid
            << ",\"quadtree_results_equal\":" << m_quadsValid << ",\"player_walked\":" << m_pathMoved
            << ",\"path_demo_end_error\":" << (m_route.empty()? -1.f : Vec3::Distance(m_player->GetTransform()->GetPosition(),m_route.back()))
            << ",\"nav_triangles\":" << m_nav->GetTriangleCount() << ",\"model_indices\":" << m_modelIndices
            << ",\"model_pass\":" << static_cast<unsigned>(m_modelPass)
            << ",\"render_phases\":" << m_renderCases.size() << ",\"vsync\":false,\"recording\":"
#if defined(DXA_GAME_RECORD_VIDEO)
            << "true}";
#else
            << "false}";
#endif
        result.flush();
        ReferenceQueueFrame(L"diagnostics-final.bmp");
        PostQuitMessage(passed?0:1);
    }

    shared_ptr<Player> m_player;
    shared_ptr<NavMesh> m_nav;
    std::vector<Vec3> m_route;
    std::vector<shared_ptr<GameObject>> m_renderObjects,m_quadObjects;
    unique_ptr<QuadTree> m_tree;
    std::ofstream m_renderCsv;
    Vec3 m_walkStart{};
    Clock::time_point m_stageAt{},m_lastTick{};
    int m_stage=-1;
    size_t m_renderIndex=0;
    unsigned m_modelIndices=0;
    uint8 m_modelPass=0;
    float m_pathMoved=0;
    std::wstring m_pendingCapture;
    Clock::time_point m_captureAt{};
    bool m_finished=false,m_moveRequested=false,m_pathsValid=true,m_quadsValid=true;
};

void ReferenceDiagnosticsTick()
{
    static ReferenceDiagnostics diagnostics;
    diagnostics.Tick();
}
