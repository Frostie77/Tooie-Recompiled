"""Ignore only LOOKUP_FUNC's physical C-line metadata in instruction parity checks."""
import re

def without_lookup_lines(source):
    pieces=[];previous=0
    for match in re.finditer(r'\btooie_lookup_func_at\(',source):
        end=match.end();depth=1
        while depth:
            depth+=(source[end]=='(')-(source[end]==')');end+=1
        call=source[match.start():end]
        call,n=re.subn(r'(,\s*__func__,\s*)\d+(?=\)$)',r'\g<1>0',call)
        assert n==int('__func__' in call),'Unexpected contextual lookup shape'
        pieces.extend([source[previous:match.start()],call]);previous=end
    return ''.join(pieces)+source[previous:]
