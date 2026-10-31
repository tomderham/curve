# 헤드폰 EQ 및 AutoEQ

Curve는 측정된 헤드폰 프로필로 Parametric EQ를 설정할 수 있으므로, Mac의 모든 오디오에서 헤드폰의 주파수 응답을 보정할 수 있습니다.

## AutoEQ 헤드폰 프로필 사용

인터넷 연결이 필요합니다.

1. 편집기 배경을 오른쪽 클릭하고 Curve Plugins > Parametric EQ (import from AutoEQ...)를 선택합니다.
2. 검색 상자에 HD 650이나 AirPods 같은 헤드폰 모델을 입력합니다.
3. 결과를 선택하면 응답 곡선과 필터를 볼 수 있습니다.
4. Use this EQ를 클릭합니다. Curve가 해당 설정으로 Parametric EQ 노드를 만듭니다.
5. [노드를 체인에 연결](editor.md)합니다.
6. 유지하려면 Save As Preset...를 선택합니다.

대신 기존 Parametric EQ 노드를 업데이트하려면 노드를 오른쪽 클릭하고 Import EQ from AutoEQ...를 선택합니다.

헤드폰 모델이 없으면 Sync Index를 클릭하여 AutoEQ 데이터베이스에 최근 추가된 항목을 확인합니다.

## 미세 조정 또는 다른 프로필 찾기

- 타깃 곡선과 음색을 조정하려면 [autoeq.app](https://autoeq.app)을 사용하고 EQ 곡선을 내보냅니다.
- 헤드폰이 목록에 없으면 커뮤니티가 측정한 헤드폰 및 IEM 데이터가 있는 [squig.link](https://squig.link)도 사용해 볼 수 있습니다.

## EQ 파일 가져오기

Curve는 AutoEQ, Squig.link, REW의 EQ 곡선을 `.txt`, `.csv`, `.tsv`, `.eq` 파일에서 가져옵니다.

- 새 노드를 만들려면 편집기 배경을 오른쪽 클릭하고 Curve Plugins > Parametric EQ (import from file...)를 선택하거나, 파일을 편집기 창으로 드래그합니다.
- 기존 Parametric EQ 노드를 업데이트하려면 노드를 오른쪽 클릭하고 Import EQ from file...를 선택합니다.

## EQ 직접 조정

Parametric EQ 노드를 이중 클릭하여 창을 엽니다.
