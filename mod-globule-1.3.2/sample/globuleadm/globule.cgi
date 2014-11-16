eval `echo $QUERY_STRING \
| tr '&' '\012' \
| sed -e 's/%20/ /g'       -e 's/%21/!/g'    -e 's/%22/"/g'     \
      -e 's/%23/#/g'       -e 's/%24/$/g'    -e 's/%25/%/g'     \
      -e 's/%26/&/g'       -e "s/%27/'/g"    -e 's/%28/(/g'     \
      -e 's/%29/)/g'       -e 's/%2[aA]/*/g' -e 's/%2[bB]/+/g'  \
      -e 's/%2[cC]/,/g'    -e 's/%2[dD]/-/g' -e 's/%2[eE]/./g'  \
      -e 's/%2[fF]/\//g'   -e 's/%3[aA]/:/g' -e 's/%3[bB]/;/g'  \
      -e 's/%3[cC]/</g'    -e 's/%3[dD]/=/g' -e 's/%3[eE]/>/g'  \
      -e 's/%3[fF]/?/g'    -e 's/%40/@/g'    -e 's/%5[bB]/[/g'  \
      -e 's/%5[cC]/\\\\/g' -e 's/%5[dD]/]/g' -e 's/%5[eE]/^/g'  \
      -e 's/%5[fF]/_/g'    -e 's/%60/\`/g'   -e 's/%7[bB]/{/g'  \
      -e 's/%7[cC]/|/g'    -e 's/%7[dD]/}/g' -e 's/%7[eE]/~/g'`
