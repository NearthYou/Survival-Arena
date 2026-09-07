#include "pch.h"
#include "ImGuiManager.h"
#include "BaseCollider.h"
#include "AABBBoxCollider.h"
#include "SphereCollider.h"
#include "Camera.h"
#include "Transform.h"

void ImGuiManager::Init()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(GAME->GetGameDesc().hWnd);
	ImGui_ImplDX11_Init(DEVICE.Get(), DC.Get());
}

void ImGuiManager::End()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void ImGuiManager::Update()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void ImGuiManager::Render()
{
	// Rendering
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiManager::ShowPickedObj()
{
	return;
	auto curPickedObj = CURSCENE->GetPickedObj();

	//메모리 영역이 유효할 경우에.
	if (!curPickedObj.expired()) {

		ImGuizmo::BeginFrame();

		//가져온 GameObject 정보를 ImGui에 넣기. 
		//Transpose.
		{
			auto vTranslate = curPickedObj.lock()->GetTransform()->GetLocalPosition();

			// 수정된 코드 - 정규화 추가
			auto vRotation = curPickedObj.lock()->GetTransform()->GetLocalRotation();
			//vRotation = (vRotation / XM_PI) * 180.f;

			//// 정규화 (-180~180도 범위)
			//auto normalizeAngle = [](float angle) -> float {
			//	while (angle > 180.0f) angle -= 360.0f;
			//	while (angle < -180.0f) angle += 360.0f;
			//	return angle;
			//	};

			//vRotation.x = normalizeAngle(vRotation.x);
			//vRotation.y = normalizeAngle(vRotation.y);
			//vRotation.z = normalizeAngle(vRotation.z);

			auto vScale = curPickedObj.lock()->GetTransform()->GetLocalScale();

			float vPos[3] = { vTranslate.x, vTranslate.y, vTranslate.z };
			float vRot[3] = { vRotation.x, vRotation.y, vRotation.z };
			float vSca[3] = { vScale.x, vScale.y, vScale.z };

			ImGui::Text("Transform");
			ImGui::SameLine();
			
			if (ImGui::InputFloat3("Local Position", vPos)) {
				vTranslate.x = vPos[0];
				vTranslate.y = vPos[1];
				vTranslate.z = vPos[2];
				curPickedObj.lock()->GetTransform()->SetLocalPosition(vTranslate);
			}

			ImGui::Text("Rotation");
			ImGui::SameLine();
		
			if (ImGui::InputFloat3("Local Rotation", vRot)) {
			
				vRotation.x = vRot[0] ;
				vRotation.y = vRot[1] ;
				vRotation.z = vRot[2] ;
				curPickedObj.lock()->GetTransform()->SetLocalRotation(vRotation);
			}

			ImGui::Text("Scale");
			ImGui::SameLine();
			if (ImGui::InputFloat3("Local Scale", vSca)) {
				vScale.x = vSca[0];
				vScale.y = vSca[1];
				vScale.z = vSca[2];
				curPickedObj.lock()->GetTransform()->SetLocalScale(vScale);
			}

			//Gizmo 사용한 방법. 
			auto camera = CURSCENE->GetMainCamera(); 

			if (camera) {
				Matrix viewMat = camera->GetCamera()->GetViewMatrix();
				Matrix projMat = camera->GetCamera()->GetProjectionMatrix();

				//기즈모 조작 모드 설정
				static ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
				static ImGuizmo::MODE mode = ImGuizmo::LOCAL;

				// 조작 모드 UI
				if (ImGui::RadioButton("Translate", operation == ImGuizmo::TRANSLATE))
					operation = ImGuizmo::TRANSLATE;
				ImGui::SameLine();
				if (ImGui::RadioButton("Rotate", operation == ImGuizmo::ROTATE))
					operation = ImGuizmo::ROTATE;
				ImGui::SameLine();
				if (ImGui::RadioButton("Scale", operation == ImGuizmo::SCALE))
					operation = ImGuizmo::SCALE;

				// 5. World Matrix 가져오기 (더 간단한 방법)
				Matrix gizmoMatrix = curPickedObj.lock()->GetTransform()->GetLocalMatrix();

				// 6. 기즈모 렌더링 영역 설정
				ImGuiIO& io = ImGui::GetIO();
				ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

				// 7. 기즈모 조작
				if (ImGuizmo::Manipulate(&viewMat._11, &projMat._11, operation, mode, &gizmoMatrix._11)) {
					// 변경된 매트릭스를 Transform에 적용
					Vec3 scale, translation, rot;
					Quaternion rotation;
					gizmoMatrix.Decompose(scale, rotation, translation);
					curPickedObj.lock()->GetTransform()->SetLocalPosition(Vec3(translation.x, translation.y, translation.z));
					rot = Transform::ToEulerAngles(rotation);
					curPickedObj.lock()->GetTransform()->SetLocalRotation(rot);
					curPickedObj.lock()->GetTransform()->SetLocalScale(Vec3(scale.x, scale.y, scale.z));
					// 회전 적용 (필요시 Quaternion을 Euler로 변환)
				}
			}
		}

		//Collider Button
		{
			auto collider = curPickedObj.lock()->GetCollider();

			if (collider != nullptr) {
				Vec3 vOffset = collider->GetOffset();
				Vec3 vOffsetScale = collider->GetOffsetScale();
				float vOff[3] = { vOffset.x, vOffset.y, vOffset.z };
				float vOffScale[3] = { vOffsetScale.x, vOffsetScale.y, vOffsetScale.z };
				ImGui::Text("Offset");
				ImGui::SameLine();
				if (ImGui::InputFloat3("ColliderOffset", vOff)) {
					vOffset.x = vOff[0];
					vOffset.y = vOff[1];
					vOffset.z = vOff[2];
					collider->SetOffset(vOffset);
				}

				ImGui::Text("Offset Scale");
				ImGui::SameLine();
				if (ImGui::InputFloat3("ColliderOffsetScale", vOffScale)) {
					vOffsetScale.x = vOffScale[0];
					vOffsetScale.y = vOffScale[1];
					vOffsetScale.z = vOffScale[2];
					collider->SetOffsetScale(vOffsetScale);
				}
			}
			else {
				if (ImGui::Button("Create AABB Collider")) {
					curPickedObj.lock()->AddComponent(make_shared<AABBBoxCollider>());
				}
				if (ImGui::Button("Create Sphere Collider")) {
					curPickedObj.lock()->AddComponent(make_shared<SphereCollider>());
				}
			}
		}
	}
}
