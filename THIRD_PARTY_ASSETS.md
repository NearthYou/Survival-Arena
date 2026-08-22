# 외부 자산 기록

확인 날짜는 2026-08-23이다. 원본 파일은 `assets/_source/`에서만 다루며 Git에 포함하지 않는다. 저장소에는 아래에 적은 변환 결과만 Git LFS로 보관한다.

## Quaternius Cyberpunk Game Kit

- 배포자: Quaternius
- 원본 안내: https://quaternius.com/packs/cyberpunkgamekit.html
- 원본 폴더: https://drive.google.com/drive/folders/1GyKLypoBxrpLN6GERF9U610Wiub48v9y
- 라이선스: CC0 1.0 Universal
- 라이선스 근거: 원본 안내 페이지와 https://quaternius.com/faq.html
- 사용 원본: `Character/Character.fbx`
- 원본 파일 ID: `1S9QYrx54_a4UX6q5-6A_-KtH5xq7arG_`
- 원본 SHA-256: `811AC6278A32C52343C65E7613B904FB5B5E179D04AAF0577CAD2D23B6705430`
- 원본 크기: 3,392,812바이트
- Drive `License.txt` SHA-256: `DE990EF6FC68CFFD7FD1AE342C4D0C823B541B8848D8F76BCA5D3339F4DE6F6E`

Drive의 `License.txt` 첫 줄에는 다른 Quaternius 팩 이름이 적혀 있다. 라이선스 본문은 CC0 1.0이고 Cyberpunk 원본 페이지도 CC0로 표시한다. 팩 이름 불일치는 이 문서에 남기고 라이선스 판단은 공식 원본 페이지와 FAQ를 기준으로 했다.

변환 명령은 다음과 같다.

```powershell
dxa_asset_tool model --input Character.fbx --output cyber-runner.dxam --sample-rate 30
```

변환 결과:

- `assets/runtime/characters/cyber-runner.dxam`
- 7,669 정점, 22,560 인덱스, 25 관절, 22 애니메이션
- SHA-256: `E4DDF7D20ACDC4D4E0C219E773945F58AD688C8DF96908AEAA3F6C326CABBEDB`

## Kenney Prototype Kit 1.0

- 배포자: Kenney
- 원본 안내: https://www.kenney.nl/assets/prototype-kit
- 다운로드 원본: https://kenney.nl/media/pages/assets/prototype-kit/4d3b7073ed-1724832076/kenney_prototype-kit.zip
- 라이선스: Creative Commons Zero, CC0 1.0 Universal
- 원본 ZIP SHA-256: `213B522FB12BCC9B9AC66C4F7581F7C74623293272212E40A70C39936AD3DA95`
- 원본 ZIP 크기: 2,961,396바이트

사용한 ZIP 내부 파일:

- `Models/FBX format/floor-thick.fbx`
  - SHA-256: `24DBA0AA319420E6215855CEADA68E3FE31C7185D2CB40DD7356C1BABF50F554`
- `Models/FBX format/Textures/colormap.png`
  - SHA-256: `0D4947D34FF32ACF4A359C7F22CA784E057E7E72F622170A9A77B6FC88FDB70E`
- `License.txt`
  - SHA-256: `EC05660E0DCA843CC52D9BDD98412BF8A1CE91606ECF23AB38C39AF71E629E62`

변환 명령은 다음과 같다.

```powershell
dxa_asset_tool model --input floor-thick.fbx --output prototype-floor.dxam
dxa_asset_tool texture --input colormap.png --output colormap.dds
```

변환 결과:

- `assets/runtime/environment/prototype-floor.dxam`
  - 24 정점, 36 인덱스
  - SHA-256: `62E03BE32EA5D008F6DFCF956116CC1071FA3B4D957D6DB56B3E25B17D63A9D2`
- `assets/runtime/environment/colormap.dds`
  - 512×512, BC7 sRGB, mip 10단계
  - SHA-256: `FC0BA7ADE908C33C087CD2E057DC82E63FF2A845B61B94B7B8BCE8B0596C1C88`

기본 BC7 encoder로 `colormap.png`를 변환하는 데 약 368초가 걸렸다. 결과를 감추지 않고 남기며, 빠른 encoder와 품질 비교는 별도 최적화 작업에서 수행한다.
