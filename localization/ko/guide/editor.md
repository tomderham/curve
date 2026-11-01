# 편집기

편집기는 프리셋을 노드 그래프로 보여 줍니다. 오디오는 각 노드 하단의 출력에서 다음 노드 상단의 입력으로 흐릅니다. 편집기를 열려면 Curve 메뉴에서 Show Editor를 선택합니다. 닫으려면 닫기 버튼을 클릭하거나 Hide Editor를 선택합니다. 어느 경우든 오디오는 계속 작동합니다.

편집기 하단의 상태 막대에는 힌트와 포인터 아래에 있는 항목의 설명이 표시됩니다. 플러그인의 경우 제조사와 포맷(예: AudioUnit 또는 VST3)도 표시됩니다.

## 노드 추가

편집기 배경을 오른쪽 클릭한 다음 다음 중에서 선택합니다.

- **Audio I/O:** 오디오 입력, 오디오 출력 및 Interface Loopback (In) 노드.
- **MIDI I/O:** MIDI 입력 및 출력 노드.
- **Curve Plugins:** [Curve의 내장 플러그인](curve-plugins.md).
- **스캔한 플러그인:** 이 메뉴들 아래에 제조사별로 표시됩니다.

플러그인 파일이나 [EQ 파일](eq-and-autoeq.md)을 편집기 창으로 드래그할 수도 있습니다.

## 노드 연결

한 노드의 핀에서 다른 노드의 핀으로 드래그합니다. 오디오 핀은 회색이고 MIDI 핀은 호박색입니다. 하나의 출력을 여러 입력에 연결할 수 있으며 원하는 체인을 자유롭게 구성할 수 있습니다.

스테레오 쌍처럼 같은 두 노드 사이를 잇는 연속된 채널은 하나의 굵은 선으로 표시되며, 각 핀으로 갈라집니다. 채널은 분리된 상태로 유지되며 믹스되지 않습니다. 선을 드래그하면 한 번에 한 채널씩 다시 연결할 수 있습니다.

## 노드 배치

노드를 드래그하여 이동합니다. 노드는 격자에 맞춰지므로 정렬된 노드 사이의 연결은 곧게 표시됩니다. 노드를 자유롭게 배치하려면 Option 키를 누른 채 드래그합니다. Option 없이 다시 드래그하면 격자로 돌아갑니다.

전체 그래프를 신호 흐름에 따라 배치하려면 배경을 오른쪽 클릭하고 Tidy Up Layout을 선택합니다. 그래프에 더 많은 공간이 필요하면 창이 화면 크기까지 커집니다. 이후 노드를 다시 이동하기 전까지 Undo Tidy Up으로 레이아웃과 창 크기를 복원할 수 있습니다.

## 노드 바이패스

노드의 전원 버튼을 클릭합니다. 다시 클릭하면 노드가 다시 활성화됩니다. 노드를 오른쪽 클릭하고 Toggle Bypass를 선택할 수도 있습니다.

## 플러그인 창 열기

노드를 이중 클릭합니다.

편집기가 닫혀 있어도 프리셋을 불러올 때마다 창을 열려면 노드를 오른쪽 클릭하고 Always Open Plug-in GUI with Preset을 선택한 다음 Save As Preset...를 사용합니다. 창은 저장할 때의 위치에 열립니다.

## 노드 메뉴

노드를 오른쪽 클릭하면 다음 옵션이 표시됩니다.

| 항목 | 기능 |
|---|---|
| Delete This Node | 노드를 제거합니다. |
| Disconnect All Pins | 노드의 모든 연결을 제거합니다. |
| Toggle Bypass | 바이패스를 켜거나 끕니다. |
| Rename This Node | 노드에 표시되는 이름을 변경합니다. |
| MIDI Learn | [MIDI Learn](midi-control.md)을 시작합니다. |
| Clear MIDI Mappings | 노드의 MIDI 매핑을 제거합니다. 매핑이 있을 때만 표시됩니다. |
| Always Open Plug-in GUI with Preset | 프리셋을 불러올 때마다 플러그인 창을 엽니다. |
| Import EQ from AutoEQ... | Parametric EQ 노드에 표시됩니다. [헤드폰 EQ 및 AutoEQ](eq-and-autoeq.md)를 참조하십시오. |
| Import EQ from file... | Parametric EQ 노드에 표시됩니다. [헤드폰 EQ 및 AutoEQ](eq-and-autoeq.md)를 참조하십시오. |
| Configure Audio I/O | 플러그인의 입력 및 출력 핀 수를 변경합니다. |
| Audio Settings... | 오디오 입력 및 출력 노드에 표시됩니다. Audio Settings를 엽니다. |
| Save Plug-in State | 플러그인 설정을 파일에 저장합니다. |
| Load Plug-in State | 파일에서 플러그인 설정을 불러옵니다. |

## 플러그인의 채널 수 변경

플러그인의 핀 수가 원하는 대로가 아니면 해당 노드를 오른쪽 클릭하고 Configure Audio I/O를 선택합니다.
