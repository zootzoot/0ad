Engine.RegisterInterface("AttackDetection");

// Message of the form { "event": { "position": { "x": 123, "z": 456 }, "time": 1, "target": 123 }.
// sent when an entity is attacked.
Engine.RegisterMessageType("EntityAttacked");

// Message of the form { "player": 1, "event": { "position": { "x": 123, "z": 456 }, "time": 1, "target": 123 }.
// sent when a new attack is detected.
Engine.RegisterMessageType("AttackDetected");
