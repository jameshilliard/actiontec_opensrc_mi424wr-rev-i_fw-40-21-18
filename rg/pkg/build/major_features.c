/****************************************************************************
 *
 * rg/pkg/build/major_features.c
 * 
 * Copyright (C) Jungo LTD 2004
 * 
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General 
 * Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02111-1307, USA.
 *
 * Developed by Jungo LTD.
 * Residential Gateway Software Division
 * www.jungo.com
 * info@jungo.com
 */

/*
 * create a list of high level features of the distribution
 *
 * Basically this code generate a file as specified in the env variable
 * "MAJOR_FEATURES_FILE". The file contains a human readable list of all
 * the features listed in config_opt.c and tagged with OPT_MAJOR_FEATURE
 * 
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "config_opt.h"
#include "create_config.h"
#include "license_text.h"

static void cfile_feature_list_open(FILE *cfile, char *file_name)
{
    fprintf(cfile, "%s%s%s\n"
	"/* This file was automatically generated. Do not modify it. */\n\n"
	"#include <stdio.h>\n\n", LICENSE_TEXT_JGPL_BEFORE, file_name,
	LICENSE_TEXT_JGPL_AFTER);
    
    fprintf(cfile, "char *major_features[] = {\n");
}

static void cfile_feature_print(FILE *cfile, char *description)
{
    fprintf(cfile, "    \"%s\",\n", description);
}

static void cfile_feature_list_close(FILE *cfile)
{
    fprintf(cfile, "    NULL\n};\n\n");
}

/* Print a list of the selected/not selected features */
static int print_major_features_by_selection(FILE *f, FILE *cfile, 
    int selected)
{
    option_t *opt;
    int opt_selected;
    char *description;
    
    /* Go through the complete features list and print the major ones */
    for (opt = &openrg_config_options[0] ; opt->token ; opt++)
    {
	/* Skip if not major feature OR if selection is not as required */
	if (!(opt->type & OPT_MAJOR_FEATURE) && !(opt->type & OPT_MODULE) &&
	    !(opt->type & OPT_HARDWARE))
	{
	    continue;
	}

	opt_selected = token_get(opt->token) ? 1 : 0;

	if (!opt_selected != !selected)
	    continue;

	if (selected && !(opt->type & selected))
	    continue;

	/* Use token and warn if no description */
	description = opt->description;

	if (!description)
	{
	    fprintf(stderr, "Warning: no description for major "
		"feature '%s'\n", opt->token);
	    description = opt->token;
	}
	/* Print */
	fprintf(f, "%-40s %s\n", description, opt->token);

	if (cfile && (opt->type & OPT_MAJOR_FEATURE))
	    cfile_feature_print(cfile, description);
    }
    return 0;
}

/* Print the major features list */
void print_major_features(void)
{
    /* Get the out filename from the env variable, open or quit if none */
    FILE *f = NULL, *cfile = NULL;

    if (!major_features_cfile)
	conf_err("MAJOR_FEATURES_CFILE is not defined\n");
    if (!major_features_file)
	conf_err("MAJOR_FEATURES_FILE is not defined\n");
	

    if (!(cfile = fopen(major_features_cfile, "wt")))
	conf_err("Failed to open '%s'\n", major_features_cfile);

    if (!(f = fopen(major_features_file, "wt")))
    {
	fclose(cfile);
	conf_err("Failed to open '%s'\n", major_features_file);
    }
    
    /* Print the header and distribution information */
    fprintf(f, "-------------------\n");
    fprintf(f, "Major features list\n");
    fprintf(f, "-------------------\n");

    if (dist && *dist)
	fprintf(f, "Distribution: %s\n", dist);

    if (hw && *hw)
	fprintf(f, "Hardware: %s\n", hw);

    if (os && *os)
	fprintf(f, "Os: %s\n", os);

    fprintf(f, "\n");
    
    /* Print included features list */
    fprintf(f, "Hardware configured in the distribution\n");
    fprintf(f, "-------------------------------------\n");

    cfile_feature_list_open(cfile, major_features_cfile);
    print_major_features_by_selection(f, cfile, OPT_HARDWARE);
    
    /* Print included features list */
    fprintf(f, "\n");
    fprintf(f, "Modules INCLUDED in the distribution\n");
    fprintf(f, "-------------------------------------\n");

    print_major_features_by_selection(f, cfile, OPT_MODULE);
    
    /* Print included features list */
    fprintf(f, "\n");
    fprintf(f, "Features INCLUDED in the distribution\n");
    fprintf(f, "-------------------------------------\n");

    print_major_features_by_selection(f, cfile, OPT_MAJOR_FEATURE);
    cfile_feature_list_close(cfile);
    
    /* Print the non-included features list */
    fprintf(f, "\n");
    fprintf(f, "Features NOT INCLUDED in the distribution\n");
    fprintf(f, "-----------------------------------------\n");
    print_major_features_by_selection(f, NULL, 0);

    /* Done */
    if (f)
	fclose(f);
    if (cfile)
	fclose(cfile);
}

