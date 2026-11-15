# 키보드 단축키

## 어디서나 Curve 메뉴 열기

전역 단축키를 사용하면 다른 앱이 앞에 있어도 Curve 메뉴를 열 수 있습니다. 설정하기 전까지는 꺼져 있습니다.

1. Settings > Preferences...를 선택하고 Keyboard 탭을 연 다음 Global Menu Shortcut 옆의 Change...를 클릭합니다.
2. Record Shortcut을 클릭한 다음 Command, Option 또는 Control이 포함된 키 조합을 누릅니다. 취소하려면 Esc를 누릅니다.
3. Save를 클릭합니다.

Reset to Default를 클릭하면 ⌥⇧C를 사용하고, Disable을 클릭하면 단축키가 꺼집니다.

## 음량 키로 Curve의 음량 조절하기

일부 오디오 인터페이스에는 macOS가 사용할 수 있는 음량 조절이 없어서 키보드의 음량 키가 아무 동작도 하지 않습니다. 대신 Curve가 이 키로 자체 출력 음량을 조절할 수 있습니다.

1. Settings > Preferences...를 선택하고 Keyboard 탭을 연 다음 Volume Keys를 When macOS Can't Control the Interface's Volume으로 설정합니다.
2. macOS가 요청하면 시스템 설정 > 개인정보 보호 및 보안 > 손쉬운 사용에서 Curve에 접근 권한을 줍니다. Curve가 키를 받으려면 이 권한이 필요합니다.

그러면 Curve가 macOS처럼 메뉴 막대의 사운드 아이콘 아래에 자체 음량 표시를 보여 줍니다.

- 음량 키는 macOS와 같이 16단계로 바뀝니다. 더 세밀하게 조절하려면 Shift와 Option을 누른 채 음량 키를 누릅니다.
- 음소거 키는 Curve의 출력을 끄고 켭니다.

Curve는 macOS의 출력이 Curve의 출력 인터페이스이고 Curve가 소리를 내는 채널의 음량을 macOS가 조절할 수 없는 동안에만 키를 가져옵니다. 음량 조절이 전혀 없는 인터페이스와, macOS의 음량 조절이 일부 채널에만 적용되고 Curve가 소리를 내는 채널에는 적용되지 않는 인터페이스가 여기에 포함됩니다. 그 밖의 경우에는 키가 macOS에서 평소처럼 동작합니다. 키로 항상 Curve의 음량을 조절하려면 대신 Always를 선택합니다.

기본적으로 Curve의 음량은 플러그인 다음에 Curve가 출력하는 모든 소리에 적용됩니다. macOS와 다른 앱의 오디오 음량만 조절하려면 Volume Keys 아래의 메뉴에서 Control Interface Loopback Volume을 선택합니다. 그러면 키는 [Interface Loopback (In)](loopback.md) 노드의 음량만 낮추므로 Audio Input과 Curve 안의 악기 플러그인 소리는 레벨이 그대로 유지됩니다. 프리셋에 연결된 Interface Loopback (In) 노드가 없으면 키는 macOS에서 평소처럼 작동합니다. 되돌리려면 Control Curve's Output Volume을 선택합니다.

Curve의 음량은 키로 조절하는 동안에만 적용됩니다. Curve는 종료해도 음량을 기억합니다. 최대 음량에서는 오디오가 전혀 바뀌지 않습니다.

음량 표시를 숨기려면 같은 메뉴에서 Show Volume Overlay를 끕니다.

## 키보드로 프리셋 전환

Curve 메뉴가 열려 있는 동안 키를 눌러 프리셋을 선택합니다. 키는 각 프리셋 옆에 표시됩니다.

| 프리셋 | 키 |
|---|---|
| 1~10 | 1~0 |
| 11~20 | Option과 1~0 |
| 21~30 | Shift와 1~0 |

예를 들어 전역 단축키를 누른 다음 2를 누르면 어떤 앱에서든 두 번째 프리셋으로 전환할 수 있습니다.

## MIDI Learn 중 편집기에서

| 키 | 동작 |
|---|---|
| Esc | MIDI Learn 끝내기 |
| Delete | 선택한 컨트롤의 매핑 제거 |
