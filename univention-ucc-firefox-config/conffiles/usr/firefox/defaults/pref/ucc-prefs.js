@%@UCRWARNING=// @%@

@!@
prefix = "ucc/firefox/defaults/"
for i in configRegistry:
        if i.startswith(prefix):
				print configRegistry[i]
@!@
