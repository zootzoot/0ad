function AttackDetection() {}

AttackDetection.prototype.Schema =
	"<a:help>Detects incoming attacks.</a:help>" +
	"<a:example/>" +
	"<a:component type='system'/>" +
	"<empty/>";

AttackDetection.prototype.Init = function()
{
	this.suppressionTransferRange = 50; // Any attacks within this range will replace the previous attack suppression.
	this.suppressionRange = 150; // Other attacks within this distance will not be registered.
	this.suppressionTime = 40 * 1000; // Other attacks within this time will not be registered.

	this.suppressedList = [];
};

// Utility function for calculating the distance between two attack events.
AttackDetection.prototype.distance = function(pos1, pos2)
{
	var xs = pos2.x - pos1.x;
	var zs = pos2.z - pos1.z;
	return Math.sqrt(Math.pow(xs, 2) + Math.pow(zs, 2));
}

AttackDetection.prototype.handleAttack = function(target, attacker)
{
	var cmpPlayer = Engine.QueryInterface(this.entity, IID_Player);
	var cmpOwnership = Engine.QueryInterface(target, IID_Ownership);
	// Don't register attacks dealt against others.
	if (cmpPlayer.GetPlayerID() != cmpOwnership.GetOwner())
		return;

	var cmpPosition = Engine.QueryInterface(target, IID_Position);
	if (!cmpPosition || !cmpPosition.IsInWorld())
		return;
	var entityPosition = cmpPosition.GetPosition();
	
	var cmpTimer = Engine.QueryInterface(SYSTEM_ENTITY, IID_Timer);
	var currentTime = cmpTimer.GetTime();

	for (var i = 0; i < this.suppressedList.length; i++)
	{
		var event = this.suppressedList[i];
		
		// Check if the current event has timed out.
		if (currentTime - event.time > this.suppressionTime)
		{
			this.suppressedList.splice(i, 1);
			i--;
			continue;
		}
		
		// If this is within suppression distance of the event then check if the event should be updated
		// and then return.
		var dist = this.distance(event.position, entityPosition);
		if (dist < this.suppressionRange)
		{
			if (dist < this.suppressionTransferRange)
			{
				event.position = {x: entityPosition.x, z: entityPosition.z};
				event.time = currentTime;
			}
			return;
		}
	}
	
	var event = {position:{x: entityPosition.x, z: entityPosition.z}, time: currentTime};
	this.suppressedList.push(event);
	Engine.PostMessage(this.entity, MT_AttackDetected, { "player": cmpPlayer.GetPlayerID(), "event": event });
	var cmpGuiInterface = Engine.QueryInterface(SYSTEM_ENTITY, IID_GuiInterface);
	cmpGuiInterface.PushNotification({"type": "attack", "player": cmpPlayer.GetPlayerID(), "message": event});
};

//// Message handlers /////

AttackDetection.prototype.OnGlobalAttacked = function(msg)
{
	this.handleAttack(msg.target, msg.attacker);
};

//// Public interface ////

AttackDetection.prototype.GetIncomingAttacks = function()
{
	return this.suppressedList;
};

Engine.RegisterComponentType(IID_AttackDetection, "AttackDetection", AttackDetection);
