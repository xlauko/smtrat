/**
 * @file EMSettings.h
 * @author YOUR NAME <YOUR EMAIL ADDRESS>
 *
 * @version 2015-09-10
 * Created on 2015-09-10.
 */

#pragma once

#include "../ModuleSettings.h"

namespace smtrat
{
    struct EMSettings1 : ModuleSettings
    {
		static constexpr auto moduleName = "EMModule<EMSettings1>";
        /**
         * Example for a setting.
         */
        static const bool example_setting = true;
    };
}
