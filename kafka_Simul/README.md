# Kafka Simulator (C#)

Windows Forms UI 기반 Kafka 메시지 전송/수신 시뮬레이터입니다.

## 구조
- `KafkaSimulator.csproj` - .NET WinForms 프로젝트 파일
- `Program.cs` - UI 앱 진입점
- `MainForm.cs` - Kafka producer/consumer UI 코드

## 실행 방법
1. `kafka_Simul` 폴더에서 프로젝트 복원
   ```powershell
   dotnet restore
   ```
2. 빌드
   ```powershell
   dotnet build
   ```
3. 앱 실행
   ```powershell
   dotnet run --project kafka_Simul/KafkaSimulator.csproj
   ```

## VS Code에서 실행
1. VS Code에서 `e:\Git_Source\NoriStudy` 워크스페이스를 엽니다.
2. `kafka_Simul` 폴더에 `.vscode/launch.json`, `.vscode/tasks.json`, `.vscode/settings.json`이 생성되어 있습니다.
3. VS Code에서 `Run and Debug` 를 열고 `Launch KafkaSimulator (Debug Build)` 또는 `Launch KafkaSimulator (Release Build)` 구성을 선택합니다.
4. `F5` 키를 눌러 앱을 실행합니다.

## 터미널에서 직접 실행
```powershell
cd kafka_Simul
& "$env:USERPROFILE\.dotnet\dotnet.exe" run --project KafkaSimulator.csproj
```

## 수동 빌드 후 실행
1. 빌드:
   ```powershell
   cd kafka_Simul
   & "$env:USERPROFILE\.dotnet\dotnet.exe" build
   ```
2. 실행:
   ```powershell
   .\bin\Debug\net7.0-windows\KafkaSimulator.exe
   ```

> 이 작업은 현재 사용자 폴더 `%USERPROFILE%\\.dotnet`에 설치된 .NET SDK를 사용합니다.

## 사용 방법
1. 브로커 주소에 Kafka 서버 주소 입력 (기본: `localhost:9092`)
2. 토픽 이름 입력 (기본: `test-topic`)
3. Producer 영역에 메시지를 입력하고 `Send` 버튼 클릭
4. Consumer 영역에서 `Start Consumer` 버튼을 눌러 메시지를 수신
5. 수신 중지하려면 `Stop Consumer` 버튼 클릭

## Docker로 Kafka 실행
`kafka_Simul` 폴더에서 다음 명령을 실행하면 Kafka와 Zookeeper가 Docker 컨테이너로 실행됩니다.
```powershell
cd kafka_Simul
docker compose up -d
```

서비스가 정상적으로 올라가면 앱에서 브로커 주소로 `localhost:9092` 를 사용하세요.

Kafka/Docker 중지를 원하면:
```powershell
docker compose down
```

## 상세 설명
- UI 상단에서 Kafka 브로커와 토픽을 설정합니다.
- Producer는 입력한 메시지를 토픽으로 전송합니다.
- Consumer는 토픽을 구독하고 도착한 메시지를 로그 목록에 표시합니다.

> Kafka 브로커가 실행 중이어야 합니다. 로컬 Kafka가 없다면 Docker로 Kafka를 실행한 뒤 테스트하세요.
