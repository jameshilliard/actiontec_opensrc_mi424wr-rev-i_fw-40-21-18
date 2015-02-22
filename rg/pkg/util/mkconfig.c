/****************************************************************************
 *
 * rg/pkg/util/mkconfig.c
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define LINE_SIZE 256
#define MAX_N_FILES 256
#define INCLUDE_COMMAND "include "

void process_line(char *line);
char *get_token(char **pline, int *state, int tok_num);

int main(int argc, char *argv[])
{
    FILE *mkfile[MAX_N_FILES];
    int mkindex = 0;
    char line[LINE_SIZE];
    if (argc<=1)
    {
	fprintf(stderr, "use: %s filename (make ver)\n", argv[0]);
	exit(1);
    }
    
    if (!(mkfile[mkindex] = fopen(argv[1], "r")))
    {
	fprintf(stderr, "can't open file %s\n", argv[1]);
	exit(2);
    }
    while (mkindex>=0)
    {
	if (!fgets(line, sizeof(line), mkfile[mkindex]))
	{
	    fclose(mkfile[mkindex]);
	    mkindex--;
	    continue;
	}
	if (!strncmp(line, INCLUDE_COMMAND, strlen(INCLUDE_COMMAND)))
	{
	    char *pname;
	    int i;
	    mkindex++;
	    if (mkindex >= MAX_N_FILES)
	    {
		fprintf(stderr, "maximum include level reached: %d\n", 
		    MAX_N_FILES);
		mkindex--;
		continue;
	    }
	    pname = strchr(line, ' ');
	    if (!pname)
	    {
		fprintf(stderr, "invalid include directive: '%s'\n", line); 
		continue;
	    }
	    for (; isspace(*pname); pname++); // skip spaces
	    for (i=1; *(pname+i) != '\0' && !isspace(*(pname+i)); i++); // find first space
	    *(pname+i) = '\0';
	    if(i==1)
	    {
		fprintf(stderr, "invalid include directive: \"%s'\n\"", line); 
		continue;	
	    }
	    mkfile[mkindex] = fopen(pname, "r");
	    if (!mkfile[mkindex])
	    {
		fprintf(stderr, "can't open file %s\n", pname);
		exit(2);
	    }
	    continue;
	}
	process_line((char*)line);
    }
    
    return 0;
}

void process_line(char *line)
{
    /* state:
     *   -1 : error
     *    0 : normal
     *    1 : in a commant
     */
    static int state=0;
    char *token[2];
    int define=1;

    if (state)
    {
	while(*line!='\n')
	{
	    if (*line=='*' && *(line+1)=='/')
	    {
		state = 0;
		line += 2;
		break;
	    }
	    line++;
	}
    }
        
    if (!*line || !line[0] || line[0]=='#' || line[0]=='\n' || state)
	return;

    token[0] = get_token(&line, &state, 1);
    
    if (!token[0])
    {
	if (state==-1)
	    goto wformat;
	return;
    }
    if (!token[0][0])
	return;
    
    if (state) 
	goto wformat;

    token[1] = get_token(&line, &state, 2);
    
    if (!token[1] || !token[1][0])
	goto wformat;

    if (!strcmp(token[1], "y") || !strcmp(token[1], "yes"))
	strcpy(token[1], "1");
    else if (!strcmp(token[1], "m"))
    {
	strcat(token[0], "_MODULE");
	token[1][0]='1';
    }
    else if (!strcmp(token[1], "no"))
    {
	define = 0;
    }
    
    printf("%s %s %s\n", define ? "#define" : "#undef", token[0], define ? token[1] : "");
    
    free(token[0]);
    free(token[1]);

    return;
    
    wformat:
	fprintf(stderr, "mkconfig: Error. format incorrect\n");
    	exit(3);
}

char *get_token(char **pline, int *state, int tok_num)
{
    int i;
    char *line;
    char *buf = (char*)malloc(LINE_SIZE);
    memset(buf, 0, LINE_SIZE);
    line = *pline;
    i = 0;
    
    while (*line)
    {
	if (*line=='#')
	    goto ret;
	
	/* skip comments */
	if (*line=='/' && *(line+1)=='*')
	{
	    *state = 1;
	    for(;;)
	    {
		line++;
		if (*line=='\n')
		    return buf;
		if (*line=='*' && *(line+1)=='/')
		{
		    *state = 0;
		    line += 2;
		    break;
		}
	    }
	    continue;
	}


	if (tok_num == 1 && (*line==' ' || *line=='\t' || *line=='\n'))
	{
	    /* found space while looking for the first token,
	     * skip whitespaces until we find '=' and return
	     */
	    while(*line && isspace(*line))
		line++;

	    if (*line=='=')
		goto ret;
	    else
		goto err;
	}
	else if (tok_num == 2 && *line == '\n')
	    goto ret;
	
	if (tok_num==1 && *line=='=')
	{
	    /* looking for the *first* token and found '='
	     * if no value found after the '=' - return error,
	     * else return the token upto the '=' and line starting
	     * at the value.
	     */
	    line++;

	    while (*line && isspace(*line))
	    	line++;
	    
	    if (!(*line))
		goto err;
	    goto ret;
	}

	buf[i] = *line;
	line++;
	i++;
    }

ret:
    *pline = line;
    buf[i+1] = 0;
    return buf;
err:
    free(buf);
    *state = -1;
    return NULL;
}		    
