XSLTP = xsltproc
CANDLE = candle
LIGHT = light
SHAVE = shave.xslt

.SUFFIXES : .msi .msm .wixobj .wxs .xml
.PHONY : all clean merge_module dev_vc testbench

%.wxs : %.xml
	$(XSLTP) $(SHAVE) $< > $@

%.wixobj : %.wxs
	$(CANDLE) $^

all : merge_module testbench dev_vc

merge_module : SDL_Pango.msm

dev_vc : merge_module SDL_Pango_Develop_VC2003.msi

testbench : merge_module SDL_Pango_testbench.msi

SDL_Pango.msm : merge_module.wixobj
	$(LIGHT) -b $(LIGHT_PATH) $^ -out $@

SDL_Pango_testbench.msi : testbench.wixobj ui.wixobj
	$(LIGHT) -b $(LIGHT_PATH) $^ -out $@

SDL_Pango_Develop_VC2003.msi : dev.wixobj ui.wixobj
	$(LIGHT) -b $(LIGHT_PATH) $^ -out $@

clean:
	rm -f *.wxs *.wixobj *.msm *.msi
