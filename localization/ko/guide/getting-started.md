# Curve 시작하기

Curve를 사용하려면 Apple 실리콘 또는 Intel 기반의 macOS Sonoma 14.2 이상이 필요합니다.

## Curve 설치

1. [최신 릴리스](https://github.com/tomderham/curve/releases)에서 Curve를 다운로드합니다.
2. `Curve.app`을 응용 프로그램 폴더에 복사하고 엽니다.

Curve는 메뉴 막대에서 실행되므로 기본적으로 아이콘이 Dock이 아닌 메뉴 막대에 나타납니다. 편집기 창은 처음 사용할 때 열립니다. 그 이후에는 Curve가 창을 열지 않고 시작됩니다.

Curve 메뉴를 열려면 메뉴 막대 아이콘을 클릭하거나 편집기 창 왼쪽 상단의 메뉴 아이콘을 클릭합니다.

## 플러그인 스캔

AUNBandEQ 같은 macOS 내장 플러그인이나 [Curve 자체 플러그인](curve-plugins.md)만 사용하려면 이 단계를 건너뜁니다.

1. Settings > Plug-in Manager를 선택합니다.
2. 왼쪽 하단의 Options...를 클릭한 다음 'Scan for new or updated AudioUnit plug-ins'를 선택합니다. 시간이 걸릴 수 있으며, macOS가 플러그인이 사용하는 폴더에 대한 Curve의 접근을 허용할지 물을 수 있습니다.
3. VST3 플러그인을 사용하는 경우 'Scan for new or updated VST3 plug-ins'로 같은 작업을 반복합니다.
4. 창을 닫습니다.

## 오디오 기기 선택

1. Settings > Audio Settings를 선택합니다.
2. 출력 인터페이스를 선택하고, [루프백](loopback.md)을 사용하지 않는 경우 입력 인터페이스도 선택합니다.
3. 원하는 채널과 샘플레이트, 버퍼 크기를 선택합니다.
4. MIDI를 사용하는 경우 MIDI 인터페이스를 선택합니다.
5. 창을 닫습니다.

## 첫 프리셋 만들기

프리셋은 편집기에서 연결된 노드의 체인입니다. 편집기가 열려 있지 않으면 Curve 메뉴에서 Show Editor를 선택합니다.

1. 활성화한 채널에 대한 녹색 핀과 함께 Audio Input, Interface Loopback (In), Audio Output 노드가 표시됩니다.
2. 편집기 배경을 오른쪽 클릭하고 플러그인을 선택합니다. 상단에 입력, 하단에 출력이 있는 노드가 나타납니다.
3. 입력 노드의 녹색 핀에서 플러그인의 입력 핀으로 드래그합니다.
4. 플러그인의 출력 핀에서 Audio Output 노드로 드래그합니다.
5. 플러그인 노드를 이중 클릭하여 창을 열고 조정합니다.
6. Curve 메뉴에서 Save As Preset...를 선택하고 프리셋 이름을 지정합니다.

프리셋에서 Interface Loopback (In) 노드를 사용하지 않는 경우 노드를 제거하거나 최소한 핀을 연결하지 않은 상태로 두십시오. 그렇지 않으면 오디오가 음소거될 수 있습니다.

특이한 라우팅에는 주의하십시오. Curve에는 피드백 루프에 대한 보호 기능이 어느 정도 있지만, 잘못된 구성은 여전히 장비나 청력을 손상시킬 수 있습니다.

## 설정 완료

- 로그인할 때 Curve를 시작하려면 Settings > Open at Login을 선택합니다.
- 자동 업데이트 확인은 기본적으로 켜져 있습니다. Settings > Auto-Check for App Updates에서 변경할 수 있습니다.

## 다음 단계

- [프리셋](presets.md)
- [헤드폰 EQ 및 AutoEQ](eq-and-autoeq.md)
- [루프백 및 시스템 오디오](loopback.md)
