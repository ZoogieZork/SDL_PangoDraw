XSLTP = xsltproc
CANDLE = candle
LIGHT = light
SHAVE = shave.xslt

%.wxs : %.xml
	$(XSLTP) $(SHAVE) $< > $@

%.wixobj : %.wxs
	$(CANDLE) $<

%.msm : %.wixobj
	$(LIGHT) -b $(LIGHT_PATH) $<

SDL_Pango.msm : merge_module.msm
	mv merge_module.msm SDL_Pango.msm

merge_module.msm : merge_module.wixobj

merge_module.wixobj : merge_module.wxs

merge_module.wxs: merge_module.xml

clean:
	rm *.wxs *.wixobj *.msm *.msi
