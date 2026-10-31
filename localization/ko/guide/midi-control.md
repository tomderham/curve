# MIDI 컨트롤

MIDI 컨트롤러나 키보드로 악기 플러그인을 연주하고, 플러그인 파라미터를 변경하고, 노드를 바이패스하고, 프리셋을 전환할 수 있습니다. [Audio Settings](audio-settings.md)의 MIDI 입력에서 활성화하십시오.

악기를 연주하거나 다른 플러그인에 MIDI를 보내려면 해당 플러그인에 연결된 MIDI Input 노드가 필요합니다. MIDI Learn으로 파라미터, 바이패스, 프리셋을 제어할 때는 MIDI Input 노드나 프리셋 내 연결이 필요하지 않습니다.

## 악기 플러그인 연주

1. 편집기 배경을 오른쪽 클릭하고 악기 플러그인을 추가합니다.
2. 편집기 배경을 오른쪽 클릭하고 MIDI I/O > MIDI Input을 선택합니다.
3. MIDI Input 노드의 빨간색 핀에서 악기 상단의 빨간색 핀으로 드래그합니다. MIDI 핀과 연결은 빨간색이고, 오디오 핀과 연결은 녹색입니다.
4. 악기의 녹색 출력 핀에서 Audio Output 노드로 드래그합니다.
5. 설정을 유지하려면 Save As Preset...를 선택합니다.

MIDI에 반응하는 이펙트 플러그인도 같은 방식으로 연결합니다. 플러그인에서 외부 기기로 MIDI를 보내려면 Audio Settings에서 해당 기기를 MIDI 출력으로 활성화한 다음, 플러그인의 빨간색 출력 핀을 MIDI Output 노드에 연결합니다.

## MIDI Learn으로 컨트롤 매핑

1. 아무 노드나 오른쪽 클릭하고 MIDI Learn을 선택합니다. 편집기에 강조 테두리가 표시되고 상태 막대에 안내가 나타납니다.
2. 제어할 대상을 선택합니다.
   - 플러그인 파라미터의 경우 플러그인 창을 열고 컨트롤을 움직입니다.
   - 노드 바이패스의 경우 해당 전원 버튼을 클릭합니다.
3. 컨트롤러의 노브, 페이더 또는 버튼을 움직입니다. 매핑이 즉시 이루어지며, 계속해서 다른 컨트롤을 매핑할 수 있습니다.
4. 끝내려면 Esc를 누르거나 MIDI Learn을 다시 선택합니다.

매핑은 프리셋과 함께 저장되므로, 유지하려면 Save As Preset...를 사용합니다.

## 매핑 제거

- 선택한 파라미터나 바이패스의 매핑을 제거하려면 MIDI Learn 중에 Delete를 누릅니다.
- 노드의 모든 매핑을 제거하려면 노드를 오른쪽 클릭하고 Clear MIDI Mappings를 선택합니다.

## 매핑된 컨트롤의 동작

- 노브와 페이더(CC)는 파라미터의 전체 범위를 사용합니다.
- 노트와 프로그램 체인지는 대상을 전환합니다.
- 바이패스의 경우 Curve는 CC 버튼이 누를 때마다 바이패스를 전환하는 모멘터리 방식인지, 바이패스가 버튼 상태를 따르는 래칭 방식인지 판단합니다.

매핑이 있는 노드에는 작은 MIDI 소켓 아이콘이 표시됩니다. 노드 위에 포인터를 올리면 상태 막대에서 매핑을 볼 수 있습니다.

매핑된 MIDI 메시지는 MIDI Input 노드를 통해 플러그인으로 전달되지 않습니다. 그 외의 MIDI는 모두 전달됩니다.

## 컨트롤러로 프리셋 전환

Settings > MIDI Control을 엽니다. 이 설정은 모든 프리셋에 적용됩니다.

- **Program Change Selects Preset:** 기본적으로 꺼져 있습니다. 프로그램 체인지 0은 메뉴의 첫 번째 프리셋을, 1은 두 번째 프리셋을 불러오며, [숫자 키 단축키](keyboard-shortcuts.md)와 같은 순서를 따릅니다.
- **Next Preset Control** 및 **Previous Preset Control:** 하나를 선택한 다음 사용할 컨트롤러 버튼을 누릅니다.
- **Clear Preset Controls:** 이 두 버튼 할당을 제거합니다.

## Curve 자체 플러그인의 컨트롤

| 플러그인 | 컨트롤 |
|---|---|
| Gain | 게인 및 음소거 |
| Pan | 팬 |
| Clipper | 실링 |
| Stereo Utility | 모드 및 폭 |
| Headphone Crossfeed | 양 |
| Audio Recorder | 녹음 시작 및 중지 |

Headphone Speaker Emulation, Invert Phase, Mid/Side Encoder, Mid/Side Decoder에는 설정이 없으므로 대신 바이패스를 매핑하십시오. Delay는 매핑할 수 없습니다.
