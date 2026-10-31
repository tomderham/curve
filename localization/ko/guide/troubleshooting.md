# 문제 해결

## 소리가 나지 않거나 일부 채널이 음소거됨

- 노드가 Audio Output까지 끝까지 연결되어 있는지 확인합니다.
- Interface Loopback (In) 노드가 프리셋에 있지만 사용하지 않는 경우 제거하거나 핀을 연결하지 않은 상태로 둡니다. [루프백](loopback.md)을 참조하십시오.
- 선택한 인터페이스를 분리한 경우 Curve는 다른 출력을 사용하지 않고 인터페이스가 돌아올 때까지 무음 상태를 유지합니다. 다른 인터페이스를 사용하려면 Audio Settings에서 선택합니다.
- 바이패스되거나 음소거된 노드가 없는지, [Audio Settings](audio-settings.md)에서 올바른 채널이 활성화되어 있는지 확인합니다.

## Curve Help에 콘텐츠를 사용할 수 없다고 표시됨

Curve Help는 Curve가 응용 프로그램 폴더에 있을 때만 열립니다. 그곳으로 옮긴 후 다시 시도하십시오.

## 연 후에 Curve를 찾을 수 없음

Curve는 메뉴 막대에서 실행되며 일반적으로 창을 열지 않습니다. 메뉴 막대 아이콘을 클릭한 다음 Show Editor를 선택합니다. Dock에도 표시하려면 Settings > Show in Dock을 켭니다.

## 프리셋 변경 사항이 사라짐

Curve는 변경 사항을 자동으로 저장하지 않습니다. Save As Preset...를 사용하여 기존 프리셋을 대치하십시오. [프리셋](presets.md)을 참조하십시오.

## 플러그인이 메뉴에 없음

Settings > Plug-in Manager를 선택하고 해당 플러그인 포맷의 스캔을 실행합니다. [Curve 시작하기](getting-started.md)를 참조하십시오.

## 플러그인의 핀 수가 잘못됨

노드를 오른쪽 클릭하고 Configure Audio I/O를 선택합니다.

## 샘플레이트가 계속 바뀜

다른 앱이 하드웨어의 샘플레이트를 변경하고 있을 수 있습니다. Curve가 강제로 되돌리게 하려면 [Audio Settings](audio-settings.md)에서 Always enforce sample rate를 켭니다.

## AirPods가 자동으로 전환되지 않음

Settings > Release audio interface when idle을 켭니다. [오디오 설정 및 기기](audio-settings.md)를 참조하십시오.

## Curve가 MIDI 컨트롤러에 반응하지 않음

Audio Settings의 MIDI 입력에서 컨트롤러가 활성화되어 있고 매핑이 프리셋과 함께 저장되었는지 확인합니다. [MIDI 컨트롤](midi-control.md)을 참조하십시오.
