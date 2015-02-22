# LaTeX tools
export DOC_SCRIPTS:=$(RGSRC)/pkg/doc/scripts/
export PATH:=/usr/local/openrg/bin:$(PATH)
export TEXMFCNF TEXMF
export LOCAL_PNG_PATH=/usr/local/share/lib/latex2html/icons/

JPEG2PS=jpeg2ps
PS2EPS=ps2eps
RUN_LATEX=$(DOC_SCRIPTS)runlatex.pl
STRIP_LATEX=$(DOC_SCRIPTS)strip_latex
CONF_FILES=$(RGSRC)/linux/.config $(BUILDDIR)/.deliver
PDFLATEX=pdflatex
DOCS_DIR=$(RGSRC)/pkg/doc
FOP=fop
XSLTPROC=xsltproc
JXMLMACRO=$(RGSRC)/build/pkg/doc/scripts/jxml_macro
XMLFMT=$(RGSRC)/pkg/doc/scripts/xmlformat.pl \
  --config-file $(RGSRC)/pkg/doc/scripts/xmlformat.conf

# latex variables

PS_SRC:=$(WORD_SRC:%.doc=%.ps) $(PPT_SRC:%.ppt=%.ps) $(VISIO_SRC:%.vsd=%.ps)
PS_EPS:=$(PS_SRC:%.ps=%.eps)
FH_EPS:=$(FH_SRC:%.fh8=%.eps)
FH_PNG:=$(FH_SRC:%.fh8=%.png)

JPEG_EPS:=$(JPEG_SRC:%.jpeg=%.eps)
BMP_EPS:=$(BMP_SRC:%.bmp=%.eps)
BMP_PNG:=$(BMP_SRC:%.bmp=%.png)
PNG_EPS:=$(PNG_SRC:%.png=%.eps)
IMAGES_EPS:=$(JPEG_EPS) $(BMP_EPS) $(PS_EPS) $(PNG_EPS)
IMAGES_PNG:=$(BMP_PNG) $(FH_PNG)

# List of tex files to produce
TEX_TARGET+=$(TEX_SRC)
TEX_TARGETS=$(TEX_TARGET:%.tex=%.pdf) $(TEX_TARGET:%.tex=%.ps) \
	$(TEX_HTML) $(TEX_SLIDE)


# Targets are also sources
TEX_SOURCE+=$(TEX_SRC) $(TEX_SRC1)
TEX_ALLSRC=$(TEX_PRINT) $(TEX_ONLINE)
TEX_PS:=$(TEX_SOURCE:%.tex=%.ps)
TEX_PDF:=$(TEX_SOURCE:%.tex=%.pdf)
TEX_HTML:=$(TEX_SOURCE:%.tex=html_%)
TEX_PRINT=$(patsubst %.tex,%_print.tex,$(TEX_SOURCE))
TEX_ONLINE=$(patsubst %.tex,%_online.tex,$(TEX_SOURCE))
TEX_PRINT_PS=$(patsubst %.tex,%_print.ps,$(TEX_SOURCE))
TEX_ONLINE_PS=$(patsubst %.tex,%_online.ps,$(TEX_SOURCE))
TEX_HTML_DVI=$(patsubst %,%.dvi,$(TEX_HTML))
TEX_PRINT_DVI=$(patsubst %.tex,%_print.dvi,$(TEX_SOURCE))
TEX_ONLINE_DVI=$(patsubst %.tex,%_online.dvi,$(TEX_SOURCE))
TEX_PRINT_PDF=$(patsubst %.tex,%_print.pdf,$(TEX_SOURCE))
TEX_UM_LINKS=$(patsubst %,$(RGSRC)/pkg/doc/user_manual/%,$(UM_TEX_LINK))
IMG_LINKS+=$(patsubst %,$(RGSRC)/pkg/doc/user_manual/images/%,$(UM_IMG_LINK))
TEX_PG_LINKS=$(patsubst %,$(RGSRC)/pkg/doc/programmer_guide/%,$(PG_TEX_LINK))
TEX_CG_LINKS=$(patsubst %,$(RGSRC)/pkg/doc/configuration_guide/%,$(CG_TEX_LINK))
ONLINE_AUX=$(TEX_SOURCE:%.tex=%_online.aux)
TEX_INCLUDE+=version.tex definitions.tex
LATEX_DEP=$(IMAGES_EPS) $(FH_EPS) $(TEX_INCLUDE) $(CONF_FILES)
IFDEFS_FILE=run_time_ifdefs.tex
	
# XML addition
export FOP_OPTS=-Xms5m -Xmx750m

XML_FMT:=$(XML_SRC:%.xml=fmt_%.xml) $(XML_TARGET:%.xml=fmt_%.xml)
XML_PDF:=$(XML_TARGET:%.xml=%.pdf)
XML_RTF:=$(XML_TARGET:%.xml=%.rtf)
XML_HTML:=$(XML_TARGET:%.xml=html_%/)
XML_HTML_WEB:=$(XML_TARGET:%.xml=%.html/)
XML_FO=$(patsubst %.xml,%.fo,$(XML_TARGET))
XML_ONLINE=$(XML_SRC:%.xml=%_online.xml) $(XML_TARGET:%.xml=%_online.xml) \
  $(UM_XML_LINK:%.xml=%_online.xml) $(PG_XML_LINK:%.xml=%_online.xml) \
  $(CG_XML_LINK:%.xml=%_online.xml)
XML_GEN_FILES=runtime_profile.xsl version.ent 
XML_DEP+=$(XML_GEN_FILES) $(DOC_SCRIPTS)/definitions.ent \
  $(DOC_SCRIPTS)/common.xsl $(DOC_SCRIPTS)/print.xsl \
  $(DOC_SCRIPTS)/html_theam.css $(DOC_SCRIPTS)/html.xsl \
  $(DOC_SCRIPTS)/html_chunk.xsl $(DOC_SCRIPTS)/html_common.xsl \
  $(DOC_SCRIPTS)/j_titlepage.xsl $(DOC_SCRIPTS)/preamble.ent 
  
XML_TARGETS=$(XML_PDF) $(XML_RTF) $(XML_HTML) $(XML_HTML_WEB)

XML_UM_LINKS=$(patsubst %,$(RGSRC)/pkg/doc/user_manual/%,$(UM_XML_LINK))
XML_PG_LINKS=$(patsubst %,$(RGSRC)/pkg/doc/programmer_guide/%,$(PG_XML_LINK))
XML_CG_LINKS=$(patsubst %,$(RGSRC)/pkg/doc/configuration_guide/%,$(CG_XML_LINK))
#########################################

ifneq ($(UM_IMG_LINK)$(IMG_LINKS),)
  ifneq ($(IS_DISTRIBUTION),y)
    ARCHCONFIG_FIRST_TASKS+=do_um_img_links
  endif
endif
ifneq ($(UM_TEX_LINK)$(TEX_UM_LINKS),)
  ARCHCONFIG_FIRST_TASKS+=do_um_tex_links
endif
ifneq ($(PG_TEX_LINK)$(TEX_PG_LINKS),)
  ARCHCONFIG_FIRST_TASKS+=do_pg_tex_links
endif
ifneq ($(CG_TEX_LINK)$(TEX_CG_LINKS),)
  ARCHCONFIG_FIRST_TASKS+=do_cg_tex_links
endif

# XML addition
ifneq ($(UM_XML_LINK)$(XML_UM_LINKS),)
  ARCHCONFIG_FIRST_TASKS+=do_um_xml_links
endif
ifneq ($(PG_XML_LINK)$(XML_PG_LINKS),)
  ARCHCONFIG_FIRST_TASKS+=do_pg_xml_links
endif
ifneq ($(CG_XML_LINK)$(XML_CG_LINKS),)
  ARCHCONFIG_FIRST_TASKS+=do_cg_xml_links
endif
#########################################

docs: $(DOCS_SUBDIRS_) $(TEX_TARGETS)
xmldocs: $(DOCS_SUBDIRS_) $(XML_TARGETS)
xmlfmt: $(XML_FMT)

# Add to clean target all documentation related files
ifneq ($(XML_TARGET)),)
  CLEAN+=$(XML_GEN_FILES) $(XML_ONLINE) $(XML_FO) $(XML_PDF) $(XML_HTML) 
  CLEAN+=$(XML_RTF) $(XML_HTML_WEB)
endif

docs_print: $(TEX_PRINT_PDF)

# latex source code compilation

$(JPEG_EPS) : %.eps : %.jpeg
	$(JPEG2PS) -o $@ $<

$(BMP_EPS) : %.eps : %.bmp
	convert $< $@

$(BMP_PNG) : %.png : %.bmp
	convert $< $@

$(PNG_EPS) : %.eps : %.png
	convert $< $@

$(FH_PNG) : %.png : %.eps
	convert $< $@

$(PS_EPS) : %.eps : %.ps
	$(PS2EPS) < $< > $@

$(TEX_ONLINE_DVI) : %.dvi : %.tex $(LATEX_DEP)
	$(call CREATE_IFDEFS_FILE,\\PRINTfalse,\\HTMLDOCfalse)
	$(RUN_LATEX) $<

$(TEX_PRINT_DVI) : %.dvi : %.tex $(LATEX_DEP)
	$(call CREATE_IFDEFS_FILE,\\PRINTtrue,\\HTMLDOCfalse)
	$(RUN_LATEX) $<

$(TEX_HTML_DVI) : %.dvi : %.tex $(LATEX_DEP)
	$(RUN_LATEX) $<
	
$(TEX_ONLINE_PS) $(TEX_PRINT_PS) : %.ps : %.dvi
	dvips -Pcmz -Pamz -j0 -o $@ $<

$(TEX_PRINT) : %_print.tex : %.tex
	cp $< $@

$(TEX_ONLINE) : %_online.tex : %.tex
	cp $< $@

$(TEX_PDF) : %.pdf : %_online.ps
	ps2pdf -dMaxSubsetPct=100 -dEmbedAllFonts=true -dSubsetFonts=true -dAutoFilterColorImages=false -dAutoFilterGrayImages=false -dColorImageFilter=/FlateEncode -dGrayImageFilter=/FlateEncode -dModoImageFilter=/FlateEncode $< $@

$(TEX_PRINT_PDF) : %.pdf : %.ps
	ps2pdf -dMaxSubsetPct=100 -dEmbedAllFonts=true -dSubsetFonts=true -dAutoFilterColorImages=false -dAutoFilterGrayImages=false -dColorImageFilter=/FlateEncode -dGrayImageFilter=/FlateEncode -dModoImageFilter=/FlateEncode $< $@

$(TEX_PS) : %.ps : %_print.ps
	cp $< $@

$(XML_FMT) : fmt_%.xml : %.xml
	$(XMLFMT) -i $<

$(XML_PDF) : %.pdf : %.fo 
	$(FOP) -fo  $<  -pdf  $@
	
$(XML_FO) : %.fo : %_online.xml $(XML_ONLINE)
	$(XSLTPROC) --xinclude --output $@ $(RGSRC)/pkg/doc/scripts/print.xsl $<
	
$(XML_ONLINE) : %_online.xml : %.xml $(XML_SRC) $(XML_UM_LINKS) $(XML_PG_LINKS) $(XML_CG_LINKS) $(XML_DEP)
	$(JXMLMACRO) $< $@
	$(XSLTPROC) --output $@ runtime_profile.xsl $@
	
$(XML_HTML) : html_% : %_online.xml $(XML_ONLINE)
	$(XSLTPROC) --xinclude --output $@/ \
	  $(RGSRC)/pkg/doc/scripts/html_chunk.xsl $<
	@cp $(RGSRC)/pkg/doc/scripts/html_theam.css $@/
	@$(MKDIR) $@/images
	@# Copy common CSS images
	@cp -a $(RGSRC)/pkg/doc/scripts/images/*.gif $@/images
	@cp -a $(RGSRC)/pkg/doc/scripts/images/*.png $@/images
	@# Copy all images avilabe in the html files to the local HTML image
	@# directory. 
	@# Search for the images in the HTML files and copy it if exsist (can
	@# be CSS images as well)
	@for f in `ls $@/*.html` ; do \
	   images=`grep png $$f | sed -e "s/^.*\<img src=\"images\///g" | \
	     sed -e "s/\".*//g"`; \
	   for cpfile in $$images ; do \
	       if [ -f images/$$cpfile ] ; then \
	         cp images/$$cpfile $@/images; \
	       fi \
	   done \
	done

$(XML_HTML_WEB) : %.html : %_online.xml $(XML_ONLINE)
	@$(MKDIR) $@/images
	$(XSLTPROC) --xinclude --output \
	  $@/$@ $(RGSRC)/pkg/doc/scripts/html.xsl $<
	@# Copy all images avilabe in the html files to the local HTML image
	@# directory. 
	@# Search for the images in the HTML files and copy it if exsist (can
	@# be CSS images as well)
	@for f in `ls $@/*.html` ; do \
	   images=`grep png $$f | sed -e "s/^.*\<img src=\"images\///g" | \
	     sed -e "s/\".*//g"`; \
	   for cpfile in $$images ; do \
	       if [ -f images/$$cpfile ] ; then \
	         cp images/$$cpfile $@/images; \
	       fi \
	   done \
	done

$(XML_RTF) : %.rtf : %.fo
	$(FOP) -fo  $<  -rtf  $@

# Making sure we have the correct version of latex2html
L2H_JUNGO_VERSION=Jungo_Patch_1.2
L2H_VERSION=$(shell latex2html --version)
IS_JUNGO_VERSION=$(filter $(L2H_JUNGO_VERSION),$(L2H_VERSION))

html_%.tex:
	cp $(@:html_%=%) $@
	$(call CREATE_IFDEFS_FILE,\\PRINTfalse,\\HTMLDOCtrue)
	texexpand $@ > $@.expand
	$(STRIP_LATEX) $@.expand > $@.stripped
	sed -e 's/[$$]\(.\)[$$]/\1/g' $@.stripped | \
	sed -e 's/[\\]dag//g' > $@
	@rm -f $@.expand $@.stripped

$(TEX_HTML): html_%: html_%.tex html_%.dvi $(if $(IS_JUNGO_VERSION),,install_latex2html)
	latex2html -address="<a href=\"http://www.jungo.com\">Jungo Software Technologies</a>" $<
	cp -a $(LOCAL_PNG_PATH) $@/
	for f in `ls $@/*.html` ; do \
	  cat $$f | sed -e 's/file:.usr.local.share.lib.latex2html.//g' > \
	  $$f.tmp;  mv -f $$f.tmp $$f; \
	done

$(TEX_SLIDE) : %.pdf : %.tex
	$(PDFLATEX) $< 
	$(PDFLATEX) $< 
	$(PDFLATEX) $< 

$(CONF_FILES):
	@touch $(BUILDDIR)/.deliver

install_latex2html:
	@echo "*** ERROR: latex2html (jungo version) is not installed !!!"
	@echo "*** Please Do: rt jpkg update latex2html"
	@false

version.tex: $(RGSRC)/pkg/include/rg_version_data.h
	$(RGSRC)/pkg/doc/scripts/version.awk < $< > $@

do_um_img_links: 
	$(RG_LN) $(IMG_LINKS) ./images

do_um_tex_links:
	$(RG_LN) $(TEX_UM_LINKS) ./

do_pg_tex_links:
	$(RG_LN) $(TEX_PG_LINKS) ./

do_cg_tex_links:
	$(RG_LN) $(TEX_CG_LINKS) ./
	
definitions.tex: 
	$(RG_LN) $(RGSRC)/pkg/doc/scripts/definitions.tex definitions.tex

# XML addition
version.ent: $(RGSRC)/pkg/include/rg_version_data.h
	$(RGSRC)/pkg/doc/scripts/j_version.awk < $< > $@
	
runtime_profile.xsl: $(RGSRC)/pkg/include/rg_version_data.h
	$(RGSRC)/pkg/doc/scripts/jprofile.sh $(DOC_PROFILE) > $@
	
do_um_xml_links:
	$(RG_LN) $(XML_UM_LINKS) ./

do_pg_xml_links:
	$(RG_LN) $(XML_PG_LINKS) ./

do_cg_xml_links:
	$(RG_LN) $(XML_CG_LINKS) ./
	
#########################################

.PHONY: install_latex2html

