var UnitTrainingPlan = function(gameState, type, metadata, number, maxMerge) {
	this.type = gameState.applyCiv(type);
	this.metadata = metadata;

	this.template = gameState.getTemplate(this.type);
	if (!this.template) {
		this.invalidTemplate = true;
		this.template = undefined;
		return;
	}
	this.category= "unit";
	this.cost = new Resources(this.template.cost(), this.template._template.Cost.Population);
	if (!number){
		this.number = 1;
	}else{
		this.number = number;
	}
	if (!maxMerge)
		this.maxMerge = 5;
	else
		this.maxMerge = maxMerge;
};

UnitTrainingPlan.prototype.canExecute = function(gameState) {
	if (this.invalidTemplate)
		return false;

	// TODO: we should probably check pop caps

	var trainers = gameState.findTrainers(this.type);

	return (trainers.length != 0);
};

UnitTrainingPlan.prototype.execute = function(gameState) {
	//warn("Executing UnitTrainingPlan " + uneval(this));
	var self = this;
	var trainers = gameState.findTrainers(this.type).toEntityArray();

	// Prefer training buildings with short queues
	// (TODO: this should also account for units added to the queue by
	// plans that have already been executed this turn)
	if (trainers.length > 0){
		trainers.sort(function(a, b) {
			var aa = a.trainingQueueTime();
			var bb = b.trainingQueueTime();
			if (a.hasClass("Civic") && !self.template.hasClass("Support"))
				aa += 0.9;
			if (b.hasClass("Civic") && !self.template.hasClass("Support"))
				bb += 0.9;
			return (aa - bb);
		});
	
		trainers[0].train(this.type, this.number, this.metadata);
	}
};

UnitTrainingPlan.prototype.getCost = function(){
	var multCost = new Resources();
	multCost.add(this.cost);
	multCost.multiply(this.number);
	return multCost;
};

UnitTrainingPlan.prototype.addItem = function(amount){
	if (amount === undefined)
		amount = 1;
	this.number += amount;
};