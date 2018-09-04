/* Net.cpp
 *
 * Copyright (C) 2017,2018 Paul Boersma
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This code is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this work. If not, see <http://www.gnu.org/licenses/>.
 */

//#include <OpenCL/OpenCL.h>
#include "Net.h"

#include "oo_DESTROY.h"
#include "Net_def.h"
#include "oo_COPY.h"
#include "Net_def.h"
#include "oo_EQUAL.h"
#include "Net_def.h"
#include "oo_CAN_WRITE_AS_ENCODING.h"
#include "Net_def.h"
#include "oo_WRITE_TEXT.h"
#include "Net_def.h"
#include "oo_READ_TEXT.h"
#include "Net_def.h"
#include "oo_WRITE_BINARY.h"
#include "Net_def.h"
#include "oo_READ_BINARY.h"
#include "Net_def.h"
#include "oo_DESCRIPTION.h"
#include "Net_def.h"

Thing_implement (RBMLayer, Layer, 0);

Thing_implement (FullyConnectedLayer, Layer, 0);

Thing_implement (Net, Daata, 0);

static autoRBMLayer RBMLayer_create (integer numberOfInputNodes, integer numberOfOutputNodes, bool inputsAreBinary) {
	try {
		autoRBMLayer me = Thing_new (RBMLayer);
		my numberOfInputNodes = numberOfInputNodes;
		my inputBiases = VECzero (numberOfInputNodes);
		my inputActivities = VECzero (numberOfInputNodes);
		my inputReconstruction = VECzero (numberOfInputNodes);
		my numberOfOutputNodes = numberOfOutputNodes;
		my outputBiases = VECzero (numberOfOutputNodes);
		my outputActivities = VECzero (numberOfOutputNodes);
		my outputReconstruction = VECzero (numberOfOutputNodes);
		my weights = MATzero (numberOfInputNodes, numberOfOutputNodes);
		my inputsAreBinary = inputsAreBinary;
		return me;
	} catch (MelderError) {
		Melder_throw (U"RBM layer with ", numberOfInputNodes, U" input nodes and ",
			numberOfOutputNodes, U" output nodes not created.");
	}
}

autoNet Net_createEmpty (integer numberOfInputNodes) {
	try {
		autoNet me = Thing_new (Net);
		
		//my numberOfInputNodes = numberOfInputNodes;
		return me;
	} catch (MelderError) {
		Melder_throw (U"Net not created.");
	}
}

void Net_initAsDeepBeliefNet (Net me, constVEC numbersOfNodes, bool inputsAreBinary) {
	if (numbersOfNodes.size < 2)
		Melder_throw (U"A deep belief net should have at least two levels of nodes.");
	integer numberOfLayers = numbersOfNodes.size - 1;
	my layers = LayerList_create ();
	for (integer ilayer = 1; ilayer <= numberOfLayers; ilayer ++) {
		autoRBMLayer layer = RBMLayer_create (
			Melder_iround (numbersOfNodes [ilayer]),
			Melder_iround (numbersOfNodes [ilayer + 1]),
			ilayer == 1 ? inputsAreBinary : true
		);
		my layers -> addItem_move (layer.move());
	}
}

autoNet Net_createAsDeepBeliefNet (constVEC numbersOfNodes, bool inputsAreBinary) {
	try {
		autoNet me = Thing_new (Net);
		Net_initAsDeepBeliefNet (me.get(), numbersOfNodes, inputsAreBinary);
		return me;
	} catch (MelderError) {
		Melder_throw (U"Net not created.");
	}
}

static void copyOutputsToInputs (Layer me, Layer you) {
	vectorcopy_preallocated (your inputActivities.get(), my outputActivities.get());
}

inline static double logistic (double excitation) {
	return 1.0 / (1.0 + exp (- excitation));
}

static void Layer_sampleOutput (Layer me) {
	for (integer jnode = 1; jnode <= my numberOfOutputNodes; jnode ++) {
		double probability = my outputActivities [jnode];
		my outputActivities [jnode] = (double) NUMrandomBernoulli (probability);
	}
}

void structRBMLayer :: v_spreadUp (kLayer_activationType activationType) {
	integer numberOfOutputNodes = our numberOfOutputNodes;
	for (integer jnode = 1; jnode <= numberOfOutputNodes; jnode ++) {
		PAIRWISE_SUM (longdouble, excitation, integer, our numberOfInputNodes,
 			double *p_inputActivity = & our inputActivities [1];
 			double *p_weight = & our weights [1] [jnode],
 			(longdouble) *p_inputActivity * (longdouble) *p_weight,
 			( p_inputActivity += 1, p_weight += numberOfOutputNodes )
		)
		excitation += our outputBiases [jnode];
		our outputActivities [jnode] = logistic ((double) excitation);
	}
	if (activationType == kLayer_activationType::STOCHASTIC)
		Layer_sampleOutput (this);
}

void Net_spreadUp (Net me, kLayer_activationType activationType) {
	for (integer ilayer = 1; ilayer <= my layers->size; ilayer ++) {
		Layer layer = my layers->at [ilayer];
		if (ilayer > 1)
			copyOutputsToInputs (my layers->at [ilayer - 1], layer);
		layer -> v_spreadUp (activationType);
	}
}

void structRBMLayer :: v_sampleInput () {
	for (integer inode = 1; inode <= our numberOfInputNodes; inode ++) {
		if (our inputsAreBinary) {
			double probability = our inputActivities [inode];
			our inputActivities [inode] = (double) NUMrandomBernoulli (probability);
		} else {   // Gaussian
			double excitation = our inputActivities [inode];
			our inputActivities [inode] = NUMrandomGauss (excitation, 1.0);
		}
	}
}

void Net_sampleInput (Net me) {
	my layers->at [1] -> v_sampleInput ();
}

void Net_sampleOutput (Net me) {
	Layer_sampleOutput (my layers->at [my layers->size]);
}

static void copyInputsToOutputs (Layer me, Layer you) {
	vectorcopy_preallocated (your outputActivities.get(), my inputActivities.get());
}

void structRBMLayer :: v_spreadDown (kLayer_activationType activationType) {
	for (integer inode = 1; inode <= our numberOfInputNodes; inode ++) {
		PAIRWISE_SUM (longdouble, excitation, integer, our numberOfOutputNodes,
 			double *p_weight = & our weights [inode] [1];
 			double *p_outputActivity = & our outputActivities [1],
 			(longdouble) *p_weight * (longdouble) *p_outputActivity,
 			( p_weight += 1, p_outputActivity += 1 )
		)
		excitation += our inputBiases [inode];
		if (our inputsAreBinary) {
			our inputActivities [inode] = logistic ((double) excitation);
		} else {   // linear
			our inputActivities [inode] = (double) excitation;
		}
	}
	if (activationType == kLayer_activationType::STOCHASTIC)
		our v_sampleInput ();
}

void Net_spreadDown (Net me, kLayer_activationType activationType) {
	for (integer ilayer = my layers->size; ilayer > 0; ilayer --) {
		Layer layer = my layers->at [ilayer];
		if (ilayer < my layers->size)
			copyInputsToOutputs (my layers->at [ilayer + 1], layer);
		layer -> v_spreadDown (activationType);
	}
}

void structRBMLayer :: v_spreadDown_reconstruction () {
	for (integer inode = 1; inode <= our numberOfInputNodes; inode ++) {
		PAIRWISE_SUM (longdouble, excitation, integer, our numberOfOutputNodes,
 			double *p_weight = & our weights [inode] [1];
 			double *p_outputActivity = & our outputActivities [1],
 			(longdouble) *p_weight * (longdouble) *p_outputActivity,
 			( p_weight += 1, p_outputActivity += 1 )
		)
		excitation += our inputBiases [inode];
		if (our inputsAreBinary) {
			our inputReconstruction [inode] = logistic ((double) excitation);
		} else {   // linear
			our inputReconstruction [inode] = (double) excitation;
		}
	}
}

void Net_spreadDown_reconstruction (Net me) {
	for (integer ilayer = my layers->size; ilayer > 0; ilayer --) {
		my layers->at [ilayer] -> v_spreadDown_reconstruction ();
	}
}

void structRBMLayer :: v_spreadUp_reconstruction () {
	integer numberOfOutputNodes = our numberOfOutputNodes;
	for (integer jnode = 1; jnode <= our numberOfOutputNodes; jnode ++) {
		PAIRWISE_SUM (longdouble, excitation, integer, our numberOfInputNodes,
 			double *p_inputActivity = & our inputReconstruction [1];
 			double *p_weight = & our weights [1] [jnode],
 			(longdouble) *p_inputActivity * (longdouble) *p_weight,
 			( p_inputActivity += 1, p_weight += numberOfOutputNodes )
		)
		excitation += our outputBiases [jnode];
		our outputReconstruction [jnode] = logistic ((double) excitation);
	}
}

void Net_spreadUp_reconstruction (Net me) {
	for (integer ilayer = 1; ilayer <= my layers->size; ilayer ++) {
		my layers->at [ilayer] -> v_spreadUp_reconstruction ();
	}
}

void structRBMLayer :: v_update (double learningRate) {
	for (integer jnode = 1; jnode <= our numberOfOutputNodes; jnode ++) {
		our outputBiases [jnode] += learningRate * (our outputActivities [jnode] - our outputReconstruction [jnode]);
	}
	for (integer inode = 1; inode <= our numberOfInputNodes; inode ++) {
		our inputBiases [inode] += learningRate * (our inputActivities [inode] - our inputReconstruction [inode]);
		for (integer jnode = 1; jnode <= our numberOfOutputNodes; jnode ++) {
			our weights [inode] [jnode] += learningRate *
				(our inputActivities [inode] * our outputActivities [jnode] -
				 our inputReconstruction [inode] * our outputReconstruction [jnode]);
		}
	}
}

void Net_update (Net me, double learningRate) {
	for (integer ilayer = 1; ilayer <= my layers->size; ilayer ++) {
		my layers->at [ilayer] -> v_update (learningRate);
	}
}

static void Layer_PatternList_applyToInput (Layer me, PatternList thee, integer rowNumber) {
	Melder_require (my numberOfInputNodes == thy nx,
		U"The number of elements in each row of ", thee, U" (", thy nx,
		U") does not match the number of input nodes of ", me, U" (", my numberOfInputNodes, U").");
	for (integer ifeature = 1; ifeature <= my numberOfInputNodes; ifeature ++) {
		my inputActivities [ifeature] = thy z [rowNumber] [ifeature];
	}
}

void Net_PatternList_applyToInput (Net me, PatternList thee, integer rowNumber) {
	try {
		Layer_PatternList_applyToInput (my layers->at [1], thee, rowNumber);
	} catch (MelderError) {
		Melder_throw (me, U" & ", thee, U": pattern ", rowNumber, U" not applied to input.");
	}
}

static void Layer_PatternList_applyToOutput (Layer me, PatternList thee, integer rowNumber) {
	Melder_require (my numberOfOutputNodes == thy nx,
		U"The number of elements in each row of ", thee, U" (", thy nx,
		U") does not match the number of output nodes of ", me, U" (", my numberOfOutputNodes, U").");
	for (integer icat = 1; icat <= my numberOfOutputNodes; icat ++) {
		my outputActivities [icat] = thy z [rowNumber] [icat];
	}
}

void Net_PatternList_applyToOutput (Net me, PatternList thee, integer rowNumber) {
	Layer_PatternList_applyToOutput (my layers->at [my layers->size], thee, rowNumber);
}

void Net_PatternList_learn (Net me, PatternList thee, double learningRate) {
	try {
		for (integer ipattern = 1; ipattern <= thy ny; ipattern ++) {
			Net_PatternList_applyToInput (me, thee, ipattern);
			Net_spreadUp (me, kLayer_activationType::STOCHASTIC);
			for (integer ilayer = 1; ilayer <= my layers->size; ilayer ++) {
				Layer layer = my layers->at [ilayer];
				layer -> v_spreadDown_reconstruction ();
				layer -> v_spreadUp_reconstruction ();
				layer -> v_update (learningRate);
			}
		}
	} catch (MelderError) {
		Melder_throw (me, U" & ", thee, U": not learned.");
	}
}

void Net_PatternList_learnByLayer (Net me, PatternList thee, double learningRate) {
	try {
		for (integer ilayer = 1; ilayer <= my layers->size; ilayer ++) {
			Layer layer = my layers->at [ilayer];
			for (integer ipattern = 1; ipattern <= thy ny; ipattern ++) {
				Layer_PatternList_applyToInput (my layers->at [1], thee, ipattern);
				my layers->at [1] -> v_spreadUp (kLayer_activationType::STOCHASTIC);
				for (integer jlayer = 2; jlayer <= ilayer; jlayer ++) {
					copyOutputsToInputs (my layers->at [jlayer - 1], my layers->at [jlayer]);
					my layers->at [jlayer] -> v_spreadUp (kLayer_activationType::STOCHASTIC);
				}
				layer -> v_spreadDown_reconstruction ();
				layer -> v_spreadUp_reconstruction ();
				layer -> v_update (learningRate);
			}
		}
	} catch (MelderError) {
		Melder_throw (me, U" & ", thee, U": not learned.");
	}
}

autoActivationList Net_PatternList_to_ActivationList (Net me, PatternList thee, kLayer_activationType activationType) {
	try {
		Layer outputLayer = my layers->at [my layers->size];
		autoActivationList activations = ActivationList_create (thy ny, outputLayer -> numberOfOutputNodes);
		for (integer ipattern = 1; ipattern <= thy ny; ipattern ++) {
			Net_PatternList_applyToInput (me, thee, ipattern);
			Net_spreadUp (me, activationType);
			NUMvector_copyElements <double> (outputLayer -> outputActivities.at, activations -> z [ipattern], 1, outputLayer -> numberOfOutputNodes);
		}
		return activations;
	} catch (MelderError) {
		Melder_throw (me, thee, U"No ActivationList created.");
	}
}

static autoMatrix Layer_extractInputActivities (Layer me) {
	try {
		autoMatrix thee = Matrix_createSimple (1, my numberOfInputNodes);
		NUMvector_copyElements <double> (my inputActivities.at, thy z [1], 1, my numberOfInputNodes);
		return thee;
	} catch (MelderError) {
		Melder_throw (me, U": input activities not extracted.");
	}
}

autoMatrix Net_extractInputActivities (Net me) {
	return Layer_extractInputActivities (my layers->at [1]);
}

static autoMatrix Layer_extractOutputActivities (Layer me) {
	try {
		autoMatrix thee = Matrix_createSimple (1, my numberOfOutputNodes);
		NUMvector_copyElements <double> (my outputActivities.at, thy z [1], 1, my numberOfOutputNodes);
		return thee;
	} catch (MelderError) {
		Melder_throw (me, U": output activities not extracted.");
	}
}

autoMatrix Net_extractOutputActivities (Net me) {
	return Layer_extractOutputActivities (my layers->at [my layers->size]);
}

autoMatrix structRBMLayer :: v_extractInputReconstruction () {
	try {
		autoMatrix thee = Matrix_createSimple (1, our numberOfInputNodes);
		NUMvector_copyElements <double> (our inputReconstruction.at, thy z [1], 1, our numberOfInputNodes);
		return thee;
	} catch (MelderError) {
		Melder_throw (this, U": input reconstruction not extracted.");
	}
}

autoMatrix Net_extractInputReconstruction (Net me) {
	return my layers->at [1] -> v_extractInputReconstruction ();
}

autoMatrix structRBMLayer :: v_extractOutputReconstruction () {
	try {
		autoMatrix thee = Matrix_createSimple (1, our numberOfOutputNodes);
		NUMvector_copyElements <double> (our outputReconstruction.at, thy z [1], 1, our numberOfOutputNodes);
		return thee;
	} catch (MelderError) {
		Melder_throw (this, U": output reconstruction not extracted.");
	}
}

autoMatrix Net_extractOutputReconstruction (Net me) {
	return my layers->at [my layers->size] -> v_extractOutputReconstruction ();
}

autoMatrix structRBMLayer :: v_extractInputBiases () {
	try {
		autoMatrix thee = Matrix_createSimple (1, our numberOfInputNodes);
		NUMvector_copyElements <double> (our inputBiases.at, thy z [1], 1, our numberOfInputNodes);
		return thee;
	} catch (MelderError) {
		Melder_throw (this, U": input biases not extracted.");
	}
}

static void Net_checkLayerNumber (Net me, integer layerNumber) {
	Melder_require (layerNumber >= 1,
		U"Your layer number (", layerNumber, U") should be positive.");
	Melder_require (layerNumber <= my layers->size,
		U"Your layer number (", layerNumber, U") should be at most my number of layers (", my layers->size, U").");
}

autoMatrix Net_extractInputBiases (Net me, integer layerNumber) {
	try {
		Net_checkLayerNumber (me, layerNumber);
		return my layers->at [layerNumber] -> v_extractInputBiases ();
	} catch (MelderError) {
		Melder_throw (me, U": input biases not extracted from layer (", layerNumber, U").");
	}
}

autoMatrix structRBMLayer :: v_extractOutputBiases () {
	try {
		autoMatrix thee = Matrix_createSimple (1, our numberOfOutputNodes);
		NUMvector_copyElements <double> (our outputBiases.at, thy z [1], 1, our numberOfOutputNodes);
		return thee;
	} catch (MelderError) {
		Melder_throw (this, U": input biases not extracted.");
	}
}

autoMatrix Net_extractOutputBiases (Net me, integer layerNumber) {
	try {
		Net_checkLayerNumber (me, layerNumber);
		return my layers->at [layerNumber] -> v_extractOutputBiases ();
	} catch (MelderError) {
		Melder_throw (me, U": output biases not extracted from layer (", layerNumber, U").");
	}
}

autoMatrix structRBMLayer :: v_extractWeights () {
	try {
		autoMatrix thee = Matrix_createSimple (our numberOfInputNodes, our numberOfOutputNodes);
		NUMmatrix_copyElements <double> (our weights.at, thy z, 1, our numberOfInputNodes, 1, our numberOfOutputNodes);
		return thee;
	} catch (MelderError) {
		Melder_throw (this, U": weights not extracted.");
	}
}

autoMatrix Net_extractWeights (Net me, integer layerNumber) {
	try {
		Net_checkLayerNumber (me, layerNumber);
		return my layers->at [layerNumber] -> v_extractWeights ();
	} catch (MelderError) {
		Melder_throw (me, U": weights not extracted from layer (", layerNumber, U").");
	}
}

autoMAT structRBMLayer :: v_getWeights () {
	return matrixcopy (our weights.get());
}

autoMAT Net_getWeights (Net me, integer layerNumber) {
	return my layers->at [layerNumber] -> v_getWeights ();
}

/* End of file Net.cpp */
