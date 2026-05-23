## Chatting-IOCP

IOCP(Input/Output Completion Port) 모델을 사용한 C++ 채팅 서버 및 클라이언트, 봇 시뮬레이터입니다.

### 1\. 주요 기능 및 요구사항

이 프로젝트는 다음 요구사항을 기반으로 구현되었습니다.

#### 서버 (ChatServer)

  * [x] **로비 입장:** 클라이언트가 연결을 요청하면, 접속 후 로비에 입장합니다.
  * [x] **채팅방 생성 & 입장:** 로비의 유저가 채팅 방 생성을 요청 시, 임의의 채팅 방을 생성 후 입장합니다.
  * [x] **채팅 기능:** 클라이언트의 위치(로비/채팅방)에 따라 해당 공간의 전체 유저에게 메시지를 브로드캐스팅합니다.
  * [x] **유저 식별:** 클라이언트는 현재 위치한 로비/방의 모든 유저를 식별하고 인지할 수 있습니다.
  * [x] **인원 제한:** 채팅 방은 최대 50명의 클라이언트만 입장 가능하며 정원이 다 차면 진입이 불가합니다.
  * [x] **랜덤 방 입장:** 클라이언트의 랜덤 방 입장 요청 시, 입장 가능한 방에 랜덤으로 입장합니다.
  * [x] **클라이언트 종료 관리:** 일정 시간(60초) 무응답 클라이언트는 세션을 정리(Timeout)합니다.
  * [x] **채팅 기록:** 모든 채팅을 인메모리 DB(`std::deque`)에 기록하며, 7일이 지난 로그는 자동 삭제합니다.

#### 클라이언트 (ChatClient / ChatBotClient)

  * [x] **수동 기능 (ChatClient):**
      * 서버 접속, 채팅, 방 생성/입장/퇴장/랜덤입장 기능을 Command(터미널) 인터페이스로 구현했습니다.
  * [x] **자동 기능 (ChatBotClient):**
      * N개의 봇이 멀티스레드로 서버에 접속하여 랜덤 행동(채팅, 방 입장/퇴장 등)을 수행합니다.
  * [x] **(선택) 행동 트리:** 봇의 자동 기능은 행동 트리(Behavior Tree) 모델과 확률 기반으로 동작합니다.

-----

### 2\. 봇 클라이언트 동작 설계 (Behavior Tree)

`ChatBotClient`는 아래와 같은 행동 트리(BT) 구조에 따라 스스로 판단하고 행동합니다.

```
(Root - Selector '?')
|
|--- (Sequence '->') : "접속 시도"
|    |
|    +-- (Condition) : IsState(Disconnected)?
|    +-- (Action)    : TryConnectToServer()
|
|--- (Sequence '->') : "로그인 시도"
|    |
|    +-- (Condition) : IsState(Connected)?
|    +-- (Action)    : SendLoginReq()
|
|--- (Sequence '->') : "로비에서 행동"
|    |
|    +-- (Condition) : IsState(InLobby)?
|    +-- (Action)    : Wait(1~3초 랜덤 대기)
|    +-- (Probabilistic Selector '%?') : "로비 행동 결정"
|        |
|        +-- [70%] (Action)    : SendLobbyChat()
|        +-- [15%] (Action)    : SendCreateRoomReq()
|        +-- [10%] (Action)    : Act_SendEnterRandomRoom()
|        +-- [5%]  (Action)    : DoNothing() (Idle)
|
|--- (Sequence '->') : "방에서 행동"
     |
     +-- (Condition) : IsState(InRoom)?
     +-- (Action)    : Wait(1~5초 랜덤 대기)
     +-- (Probabilistic Selector '%?') : "방 행동 결정"
         |
         +-- [80%] (Action)    : SendRoomChat()
         +-- [15%] (Action)    : SendLeaveRoomReq()
         +-- [5%]  (Action)    : DoNothing() (Idle)
```

-----

### 3\. 빌드 방법

  * Visual Studio 2022 (v143 C++ 17 이상) 환경에서 개발되었습니다.
  * `Chat.sln` 파일을 열어 **'Release | x64'** 구성으로 빌드하는 것을 권장 드립니다.
  * 모든 프로젝트(`ChatServer`, `ChatClient`, `ChatBotClient`)는 `Common` 프로젝트를 의존하고 있습니다.

-----

### 4\. 실행 방법 (바이너리)

제출된 `Binary` 폴더의 실행 파일을 사용하거나, 위에서 직접 빌드한 `x64\Release` 폴더의 `.exe` 파일을 사용합니다.
Windows 터미널(CMD 또는 PowerShell)을 열어 다음 순서로 실행합니다.

1.  **서버 실행** (첫 번째 터미널)

    > 서버가 9000번 포트에서 수신 대기를 시작합니다.

    ```shell
    .\ChatServer.exe
    ```

2.  **클라이언트 실행** (두 번째 터미널)

    > 아이디 입력 후 채팅 및 명령어를 사용할 수 있습니다.

    ```shell
    .\ChatClient.exe
    ```

3.  **(선택) 봇 클라이언트 실행** (세 번째 터미널)

    > 부하 테스트용 봇을 실행합니다.

    ```shell
    .\ChatBotClient.exe
    ```

    (실행할 봇의 수(N)를 입력하라는 메시지가 나옵니다.)
