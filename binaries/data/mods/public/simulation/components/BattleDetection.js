function BattleDetection() {}

BattleDetection.prototype.Schema =
	"<a:help>Detects the occurence of battles.</a:help>" +
	"<a:example/>" +
	"<a:component type='system'/>" +
	"<empty/>";

BattleDetection.prototype.Init = function()
{
};

Engine.RegisterComponentType(IID_BattleDetection, "BattleDetection", BattleDetection);
