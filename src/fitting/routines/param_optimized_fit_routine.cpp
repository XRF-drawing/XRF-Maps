/***
Copyright (c) 2016, UChicago Argonne, LLC. All rights reserved.

Copyright 2016. UChicago Argonne, LLC. This software was produced
under U.S. Government contract DE-AC02-06CH11357 for Argonne National
Laboratory (ANL), which is operated by UChicago Argonne, LLC for the
U.S. Department of Energy. The U.S. Government has rights to use,
reproduce, and distribute this software.  NEITHER THE GOVERNMENT NOR
UChicago Argonne, LLC MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR
ASSUMES ANY LIABILITY FOR THE USE OF THIS SOFTWARE.  If software is
modified to produce derivative works, such modified software should
be clearly marked, so as not to confuse it with the version available
from ANL.

Additionally, redistribution and use in source and binary forms, with
or without modification, are permitted provided that the following
conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.

    * Neither the name of UChicago Argonne, LLC, Argonne National
      Laboratory, ANL, the U.S. Government, nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY UChicago Argonne, LLC AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL UChicago
Argonne, LLC OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
***/

/// Initial Author <2016>: Arthur Glowacki



#include "param_optimized_fit_routine.h"

#include <iostream>
#include <algorithm>
#include <math.h>

#include <string.h>

#define SQRT_2xPI (real_t)2.506628275 // sqrt ( 2.0 * M_PI )

using namespace data_struct;

namespace fitting
{
namespace routines
{

// ----------------------------------------------------------------------------

Param_Optimized_Fit_Routine::Param_Optimized_Fit_Routine() : Base_Fit_Routine()
{

    _optimizer = nullptr;
    _energy_range.min = 0;
    _energy_range.max = 1999;
    _update_coherent_amplitude_on_fit = true;

}

// ----------------------------------------------------------------------------

Param_Optimized_Fit_Routine::~Param_Optimized_Fit_Routine()
{

}

// ----------------------------------------------------------------------------

void Param_Optimized_Fit_Routine::_add_elements_to_fit_parameters(Fit_Parameters *fit_params,
                                                                  const Spectra * const spectra,
                                                                  const Fit_Element_Map_Dict * const elements_to_fit)
{

    real_t this_factor = (real_t)8.0;

    for (auto el_itr : *elements_to_fit)
    {
        if( false == fit_params->contains(el_itr.first) )
        {
            real_t e_guess = (real_t)1.0e-10;

            Fit_Element_Map *element = el_itr.second;
            std::vector<Element_Energy_Ratio> energies = element->energy_ratios();
            //if element counts is not in fit params structure, add it
            if( false == fit_params->contains(el_itr.first) )
            {
                Fit_Param fp(element->full_name(), (real_t)-11.0, 300, e_guess, (real_t)0.1, E_Bound_Type::FIT);
                (*fit_params)[el_itr.first] = fp;
            }
            if(spectra != nullptr  && energies.size() > 0)
            {
                real_t e_energy = element->energy_ratios()[0].energy;
                real_t min_e =  e_energy - (real_t)0.1;
                real_t max_e =  e_energy + (real_t)0.1;

                struct Range energy_range = get_energy_range(min_e, max_e, spectra->size(), fit_params->at(STR_ENERGY_OFFSET).value, fit_params->at(STR_ENERGY_SLOPE).value);

                real_t sum = spectra->segment(energy_range.min, energy_range.count()).sum();
                sum /= energy_range.count();
                e_guess = std::max( sum * this_factor + (real_t)0.01, (real_t)1.0);
                //e_guess = std::max( (spectra->mean(energy_range.min, energy_range.max + 1) * this_factor + (real_t)0.01), 1.0);
                e_guess = std::log10(e_guess);

                (*fit_params)[el_itr.first].value = e_guess;
            }
            else
            {
                e_guess = std::log10(e_guess);
                (*fit_params)[el_itr.first].value = e_guess;
            }
        }
    }

    if( false == fit_params->contains(STR_NUM_ITR) )
    {
        //add number of iteration it took
        Fit_Param fp(STR_NUM_ITR, (real_t)-1.0, 999999, 0.0, (real_t)0.00001, E_Bound_Type::FIXED);
        (*fit_params)[STR_NUM_ITR] = fp;
    }
    if (false == fit_params->contains(STR_RESIDUAL))
    {
        //add number of iteration it took
        Fit_Param fp(STR_RESIDUAL, (real_t)-1.0, 999999, 0.0, (real_t)0.00001, E_Bound_Type::FIXED);
        (*fit_params)[STR_RESIDUAL] = fp;
    }
    (*fit_params)[STR_NUM_ITR].value = 0.0;
    (*fit_params)[STR_RESIDUAL].value = 0.0;
}

// ----------------------------------------------------------------------------

void Param_Optimized_Fit_Routine::_calc_and_update_coherent_amplitude(Fit_Parameters* fitp,
                                                                      const Spectra * const spectra)
{
    //STR_COHERENT_SCT_ENERGY
    //STR_COHERENT_SCT_AMPLITUDE
    real_t min_e = fitp->at(STR_COHERENT_SCT_ENERGY).value - (real_t)0.4;
    real_t max_e = fitp->at(STR_COHERENT_SCT_ENERGY).value + (real_t)0.4;
    real_t this_factor = (real_t)8.0;
    fitting::models::Range energy_range = fitting::models::get_energy_range(min_e, max_e, spectra->size(), fitp->value(STR_ENERGY_OFFSET), fitp->value(STR_ENERGY_SLOPE));
    size_t e_size = (energy_range.max + 1) - energy_range.min;
    real_t sum = spectra->segment(energy_range.min, e_size).sum();
    sum /= energy_range.count();
    real_t e_guess = std::max(sum * this_factor + (real_t)0.01, (real_t)1.0);
    real_t logval = std::log10(e_guess);
    (*fitp)[STR_COMPTON_AMPLITUDE].value = logval;
    (*fitp)[STR_COHERENT_SCT_AMPLITUDE].value = logval;

}

// ----------------------------------------------------------------------------

std::unordered_map<std::string, real_t> Param_Optimized_Fit_Routine::fit_spectra(const models::Base_Model * const model,
                                                                                 const Spectra * const spectra,
                                                                                 const Fit_Element_Map_Dict * const elements_to_fit)
{
    //int xmin = np.argmin(abs(x - (fitp.g.xmin - fitp.s.val[keywords.energy_pos[0]]) / fitp.s.val[keywords.energy_pos[1]]));
    //int xmax = np.argmin(abs(x - (fitp.g.xmax - fitp.s.val[keywords.energy_pos[0]]) / fitp.s.val[keywords.energy_pos[1]]));
    // fitp.g.xmin = MIN_ENERGY_TO_FIT
    // fitp.g.xmax = MAX_ENERGY_TO_FIT

    std::unordered_map<std::string, real_t> counts_dict;
    Fit_Parameters fit_params = model->fit_parameters();
    //Add fit param for number of iterations
    fit_params.add_parameter(Fit_Param(STR_NUM_ITR));
    _add_elements_to_fit_parameters(&fit_params, spectra, elements_to_fit);
    if(_update_coherent_amplitude_on_fit)
    {
        _calc_and_update_coherent_amplitude(&fit_params, spectra);
    }

    //If the sum of the spectra we are trying to fit to is zero then set out counts to -10.0 == log(0.0000000001)
    if(spectra->sum() == 0)
    {

        fit_params.set_all_value(-10.0, E_Bound_Type::FIT);

        for (auto el_itr : *elements_to_fit)
        {
            counts_dict[el_itr.first] = -10.0;
        }
        return counts_dict;
    }

    if(_optimizer != nullptr)
    {
        _optimizer->minimize(&fit_params, spectra, elements_to_fit, model, _energy_range);

        //Save the counts from fit parameters into fit count dict for each element
        for (auto el_itr : *elements_to_fit)
        {
            real_t value =  fit_params.at(el_itr.first).value;
            //convert from log10
            value = std::pow((real_t)10.0, value);
            counts_dict[el_itr.first] = value;
        }

        //model->update_fit_params_values(fit_params);

        //check if we are saving the number of iterations and save if so
        if(fit_params.contains(STR_NUM_ITR))
        {
            counts_dict[STR_NUM_ITR] = fit_params.at(STR_NUM_ITR).value;
        }
        if (fit_params.contains(STR_RESIDUAL))
        {
            counts_dict[STR_RESIDUAL] = fit_params.at(STR_RESIDUAL).value;
        }
    }

    return counts_dict;
}

// ----------------------------------------------------------------------------

Fit_Parameters Param_Optimized_Fit_Routine::fit_spectra_parameters(const models::Base_Model * const model,
                                                        const Spectra * const spectra,
                                                        const Fit_Element_Map_Dict * const elements_to_fit)
{
    //int xmin = np.argmin(abs(x - (fitp.g.xmin - fitp.s.val[keywords.energy_pos[0]]) / fitp.s.val[keywords.energy_pos[1]]));
    //int xmax = np.argmin(abs(x - (fitp.g.xmax - fitp.s.val[keywords.energy_pos[0]]) / fitp.s.val[keywords.energy_pos[1]]));

    Fit_Parameters fit_params = model->fit_parameters();
    //Add fit param for number of iterations
    fit_params.add_parameter(Fit_Param(STR_NUM_ITR, 0.0));
    fit_params.add_parameter(Fit_Param(STR_RESIDUAL, 0.0));
    _add_elements_to_fit_parameters(&fit_params, spectra, elements_to_fit);
    if(_update_coherent_amplitude_on_fit)
    {
        _calc_and_update_coherent_amplitude(&fit_params, spectra);
    }

    //If the sum of the spectra we are trying to fit to is zero then set out counts to -10.0 == log(0.0000000001)
    if(spectra->sum() == 0)
    {
        fit_params.set_all_value(-10.0, E_Bound_Type::FIT);
    }
    else
    {
        if(_optimizer != nullptr)
        {
            _optimizer->minimize(&fit_params, spectra, elements_to_fit, model, _energy_range);
        }
    }

    return fit_params;
}


// ----------------------------------------------------------------------------

void Param_Optimized_Fit_Routine::initialize(models::Base_Model * const model,
                                             const Fit_Element_Map_Dict * const elements_to_fit,
                                             const struct Range energy_range)
{
    _energy_range = energy_range;
}

// ----------------------------------------------------------------------------

void Param_Optimized_Fit_Routine::set_optimizer(Optimizer *optimizer)
{

    _optimizer = optimizer;

}

// ----------------------------------------------------------------------------

} //namespace routines
} //namespace fitting
