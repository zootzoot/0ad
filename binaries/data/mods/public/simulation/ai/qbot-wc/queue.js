/*
 * Holds a list of wanted items to train or construct
 */

var Queue = function() {
	this.queue = [];
	this.outQueue = [];
};

Queue.prototype.empty = function() {
	this.queue = [];
	this.outQueue = [];
};


Queue.prototype.addItem = function(plan) {
	for (var i in this.queue)
	{
		if (plan.category === "unit" && this.queue[i].type == plan.type && this.queue[i].number + plan.number <= this.queue[i].maxMerge)
		{
			this.queue[i].addItem(plan.number)
			return;
		}
	}
	this.queue.push(plan);
};

Queue.prototype.getNext = function() {
	if (this.queue.length > 0) {
		return this.queue[0];
	} else {
		return null;
	}
};

Queue.prototype.outQueueNext = function(){
	if (this.outQueue.length > 0) {
		return this.outQueue[0];
	} else {
		return null;
	}
};

Queue.prototype.outQueueCost = function(){
	var cost = new Resources();
	for (var key in this.outQueue){
		cost.add(this.outQueue[key].getCost());
	}
	return cost;
};

Queue.prototype.queueCost = function(){
	var cost = new Resources();
	for (var key in this.queue){
		cost.add(this.queue[key].getCost());
	}
	return cost;
};

Queue.prototype.nextToOutQueue = function(){
	if (this.queue.length > 0){
		this.outQueue.push(this.queue.shift());
	}
};

Queue.prototype.executeNext = function(gameState) {
	if (this.outQueue.length > 0) {
		this.outQueue.shift().execute(gameState);
		return true;
	} else {
		return false;
	}
};

Queue.prototype.length = function() {
	return this.queue.length;
};

Queue.prototype.countQueuedUnits = function(){
	var count = 0;
	for (var i in this.queue){
		count += this.queue[i].number;
	}
	return count;
};

Queue.prototype.countOutQueuedUnits = function(){
	var count = 0;
	for (var i in this.outQueue){
		count += this.outQueue[i].number;
	}
	return count;
};

Queue.prototype.countTotalQueuedUnits = function(){
	var count = 0;
	for (var i in this.queue){
		count += this.queue[i].number;
	}
	for (var i in this.outQueue){
		count += this.outQueue[i].number;
	}
	return count;
};
Queue.prototype.countTotalQueuedUnitsWithClass = function(classe){
	var count = 0;
	for (var i in this.queue){
		if (this.queue[i].template && this.queue[i].template.hasClass(classe))
			count += this.queue[i].number;
	}
	for (var i in this.outQueue){
		if (this.outQueue[i].template && this.outQueue[i].template.hasClass(classe))
			count += this.outQueue[i].number;
	}
	return count;
};
Queue.prototype.countTotalQueuedUnitsWithMetadata = function(data,value){
	var count = 0;
	for (var i in this.queue){
		if (this.queue[i].metadata[data] && this.queue[i].metadata[data] == value)
			count += this.queue[i].number;
	}
	for (var i in this.outQueue){
		if (this.outQueue[i].metadata[data] && this.outQueue[i].metadata[data] == value)
			count += this.outQueue[i].number;
	}
	return count;
};

Queue.prototype.totalLength = function(){
	return this.queue.length + this.outQueue.length;
};

Queue.prototype.outQueueLength = function(){
	return this.outQueue.length;
};

Queue.prototype.countAllByType = function(t){
	var count = 0;
	
	for (var i = 0; i < this.queue.length; i++){
		if (this.queue[i].type === t){
			count += this.queue[i].number;
		} 
	}
	for (var i = 0; i < this.outQueue.length; i++){
		if (this.outQueue[i].type === t){
			count += this.outQueue[i].number;
		} 
	}
	return count;
};