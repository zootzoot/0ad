Engine.RegisterInterface("BattleDetection");

// Message of the form { "to": "STATE.NAME" }.
// sent whenever the battle state changes
Engine.RegisterMessageType("BattleDetectionUpdate");
