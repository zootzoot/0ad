Engine.RegisterInterface("AttackDetection");

// Message of the form { "player": 1, "event": { "position": { "x": 0, "z": 0 }, "time": 0 }.
// sent when a new attack is detected.
Engine.RegisterMessageType("AttackDetected");
