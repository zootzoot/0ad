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

AttackDetection.prototype.addSuppression = function(event)
{
	this.suppressedList.push(event);

	var cmpTimer = Engine.QueryInterface(SYSTEM_ENTITY, IID_Timer);
	cmpTimer.SetTimeout(this.entity, IID_AttackDetection, "HandleTimeout", this.suppressionTime);
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
	var cmpTimer = Engine.QueryInterface(SYSTEM_ENTITY, IID_Timer);
	var event = {target: target, position: cmpPosition.GetPosition(), time: cmpTimer.GetTime()};

	Engine.PostMessage(target, MT_EntityAttacked, {entity: target, player: cmpOwnership.GetOwner()});

	for (var i = 0; i < this.suppressedList.length; i++)
	{
		var element = this.suppressedList[i];
		
		// If the new attack is within suppression distance of this element then check if the element should be updated
		// and then return.
		var dist = this.distance(element.position, event.position);
		if (dist < this.suppressionRange)
		{
			if (dist < this.suppressionTransferRange)
				element = event;
			return;
		}
	}
	
	this.addSuppression(event);
	Engine.PostMessage(this.entity, MT_AttackDetected, { "player": cmpPlayer.GetPlayerID(), "event": event });
	var cmpGuiInterface = Engine.QueryInterface(SYSTEM_ENTITY, IID_GuiInterface);
	cmpGuiInterface.PushNotification({"type": "attack", "player": cmpPlayer.GetPlayerID(), "message": event});
};

//// External interface ////

AttackDetection.prototype.AttackAlert = function(target, attacker)
{
	this.handleAttack(target, attacker);
};

AttackDetection.prototype.GetSuppressionTime = function()
{
	return this.suppressionTime;
};

AttackDetection.prototype.HandleTimeout = function()
{
	var cmpTimer = Engine.QueryInterface(SYSTEM_ENTITY, IID_Timer);
	for (var i = 0; i < this.suppressedList.length; i++)
	{
		var event = this.suppressedList[i];
		
		// Check if this event has timed out.
		if (cmpTimer.GetTime() - event.time >= this.suppressionTime)
		{
			this.suppressedList.splice(i, 1);
			i--;
			return;
		}
	}
};

AttackDetection.prototype.GetIncomingAttacks = function()
{
	return this.suppressedList;
};

Engine.RegisterComponentType(IID_AttackDetection, "AttackDetection", AttackDetection);
