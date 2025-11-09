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
|        +-- [10%] (Action)    : Act_SendEnterRandomRoom() // [업데이트]
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
