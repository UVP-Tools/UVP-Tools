#!/bin/bash
###############################################################################
### error definition
###############################################################################
ERR_OK=0
ERR_NG=1
ERR_FILE_NOEXIST=20
Info='eval 2>&1 logger "[CheckKernelUpdate:$FUNCNAME:$LINENO]"'
grub_config()
{
    ### grub config
    if [ -f '/boot/grub/grub.conf' ]
    then
        MENULIST=$(readlink -f "/boot/grub/grub.conf")
    elif [ -f '/boot/grub/menu.lst' ]
    then
        MENULIST=$(readlink -f '/boot/grub/menu.lst')
    elif [ -f '/boot/grub/grub.cfg' ]
    then
        MENULIST=$(readlink -f '/boot/grub/grub.cfg')
        SYS="debian"
    elif [ -f '/boot/burg/burg.cfg' ]
    then
        MENULIST=$(readlink -f '/boot/burg/burg.cfg')
        SYS="debian"
    elif [ -f '/boot/grub2/grub.cfg' ]
    then
        MENULIST=$(readlink -f '/boot/grub2/grub.cfg')
        GRUBENV=$(readlink -f '/boot/grub2/grubenv')
        SYS="grub2"
    elif [ -f '/boot/efi/EFI/redhat/grub.conf' ]
    then
        MENULIST=$(readlink -f '/boot/efi/EFI/redhat/grub.conf')
    elif [ -f '/etc/elilo.conf' ]
    then
        SYS="elilo"
        MENULIST=$(readlink -f '/etc/elilo.conf')
    fi
}

info_boot_entry()
{
    local menu_lst=$1
    entry_num=$2
    local var_list="menu_default menu_start begin_line end_line title_line initrd_line"
    local comment="init"
    
    ### set default parameter
    if [ ! -f "$menu_lst" ]
    then
        $Info "can not find $menu_lst."
        return $ERR_FILE_NOEXIST
    fi

    menu_default=$(sed -n '/\b\s*default\s*=\{0,1\}"\{0,1\}\s*[0-9]"\{0,1\}\+\s*$/=' $menu_lst | sed -n '$p')
    if [ -z "$entry_num" ]
    then
        if [ -n "$menu_default" ]
        then
        if [ -z "$SYS" ]
            then
                entry_num=$(($(sed -n "${menu_default}p" $menu_lst | sed "s/^\s*default\s*=\{0,1\}\s*\([0-9]\+\)\s*$/\1/g")+1))
            else
                entry_num=$(($(sed -n "${menu_default}p" $menu_lst | sed "s/\b\s*set\s*default\s*=\{0,1\}\s*\"\{0,1\}\([0-9]\+\)\"\{0,1\}\s*$/\1/g")+1))
            fi
        else
            $Info "can not find default boot entry in $menu_lst."
            return $ERR_NG
        fi
        $Info "entry_num is $entry_num."
    elif [ $entry_num -le 0 ]
    then
        return 1
    fi
    
    ### initialize all variables
    for var in $var_list
    do
        unset $var
    done
    
    if [ -z "$SYS" ]
    then
        menu_start=$(sed -n '/^\s*title\s\+/=' "${menu_lst}" | sed -n '1p')
    else
        menu_start=$(sed -n '/^\s*menuentry\s\+/=' "${menu_lst}" | sed -n '1p')
    fi
    if [ -z "$menu_start" ]
    then
        menu_start=$(sed -n '$=' $menu_lst)
        return 0
    fi
    
    ## process grub entry top
    # get [title_line] of grub entry
    if [ -z "$SYS" ]
    then
        title_line=$(grep -n "^[[:space:]]*title[[:space:]]\+" $menu_lst | sed -n "${entry_num}p" | awk -F ":" '{print $1}')
    else
        title_line=$(grep -n "^[[:space:]]*menuentry[[:space:]]\+" $menu_lst | sed -n "${entry_num}p" | awk -F ":" '{print $1}')
    fi
    if [ -z "$title_line" ]
    then
        $Info "can not find boot entry ${entry_num} in $menu_lst."
        return $ERR_NG
    fi
    # get [begin_line] of grub entry
    comment=$(sed -n "$(($title_line-1))p" $menu_lst | grep "^[[:space:]]*###.*YaST.*identifier:.*linux[[:space:]]*###[[:space:]]*$")
    if [ -n "$comment" ] # include grub comment above
    then
        begin_line=$((title_line-1))
    else
        begin_line=$title_line
    fi
    ## process grub entry bottom
    # get [end_line] of grub entry
    if [ -z "$SYS" ]
    then
        end_line=$(grep -n "^[[:space:]]*title[[:space:]]\+" $menu_lst | sed -n "$(($entry_num+1))p" | awk -F ":" '{print $1}')
    else
        end_line=$(grep -n "^[[:space:]]*menuentry[[:space:]]\+" $menu_lst | sed -n "$(($entry_num+1))p" | awk -F ":" '{print $1}')
    fi
    [ -z "$end_line" ] && end_line=$(sed -n '$=' "$menu_lst")
    while [ -n "$(sed -n ${end_line}s/\(^\s*#.*\)/\1/p $menu_lst)" ] # omit grub comment below
    do
        end_line=$((end_line-1))
    done
    $Info "begin_line is $begin_line end_line is $end_line."
    ## get other line of grub entry
    for ((line_num=$begin_line; line_num<=$end_line; line_num++))
    do
        line=$(sed -n "${line_num}p" $menu_lst)
        if [ -n "$(echo -n "$line" | grep "^[[:space:]]*initrd[[:space:]]\+")" ] ; then initrd_line=$line_num ; fi
    done
    
    ### return susess
    return $ERR_OK
}

info_boot_entry_uefi()
{
    local menu_lst=$1
    entry_num=$2
    local comment="init"
    local var_list="menu_default menu_start begin_line end_line root_line kernel_line initrd_line"

    ### set default parameter
    if [ ! -f "$menu_lst" ]
    then
        $Info "can not find $menu_lst."
        return $ERR_FILE_NOEXIST
    fi

    menu_default=$(sed -n '/^\s*default\s*=\{0,1\}\s*\+\S\+\s*$/=' $menu_lst | sed -n '$p')

    if [ -n "$menu_default" ]
    then
        entry_num=$(sed -n "${menu_default}p" $menu_lst | sed "s/^\s*default\s*=\{0,1\}\s*\(\S\+\)\s*$/\1/g")
    else
        $Info "can not find default boot entry in $menu_lst."
        return $ERR_NG
    fi

    ### initialize all variables
    for var in $var_list
    do
        unset $var
    done

    menu_start=$(sed -n '/^\s*label\s\+/=' "${menu_lst}" | sed -n '1p')
    if [ -z "$menu_start" ]
    then
        menu_start=$(sed -n '$=' $menu_lst)
        return 0
    fi

    label_line=$(grep -n "^[[:space:]]*label[[:space:]]\+" $menu_lst | awk -F ":" '{print $1}')
    for line in $label_line
    do
        if [ -n "$begin_line" ]
        then
            end_line=$line
            break
        fi
        lable_val=$(sed -n "${line}p" $menu_lst | sed "s/^\s*label\s*=\{0,1\}\s*\(\S\+\)\s*$/\1/g")
        if [ "$lable_val" = "$entry_num" ]
        then
            begin_line=$line
        fi
    done

    while [ -z "$(sed -n ${begin_line}s/\^[[:space:]]*$/\1/p $menu_lst)" ]
    do
        begin_line=$((begin_line-1))
    done
    if [ -z "$end_line" ]
    then
        end_line=$(sed -n '$=' "$menu_lst")
    else
        while [ -z "$(sed -n ${end_line}s/\^[[:space:]]*$/\1/p $menu_lst)" ]
        do
             end_line=$((end_line-1))
        done
    fi

    ## get other line of grub entry
    for ((line_num=$begin_line; line_num<=$end_line; line_num++))
    do
        line=$(sed -n "${line_num}p" $menu_lst)
        if [ -n "$(echo -n "$line" | grep "^[[:space:]]*initrd[[:space:]]\+")" ] ; then initrd_line=$line_num ; fi
    done

    return 0
}

info_boot_entry_grub2()
{
    local menu_lst=$1
    local grubenv=$2
    local var_list="default_entry end_line label_line lable_val tmpline initrd_line"

    if [ ! -f "$menu_lst" ]
    then
        $Info "can not find $menu_lst."
        return $ERR_FILE_NOEXIST
    fi
    if [ ! -f "$grubenv" ]
    then
        $Info "can not find $grubenv."
        return $ERR_FILE_NOEXIST
    fi

    ### initialize all variables
    for var in $var_list
    do
        unset $var
    done

    ### set default parameter
    default_entry=$(sed -n '/saved_entry/p' $grubenv | sed -n 's/saved_entry=//gp')
    if [ -z "$default_entry" ]
    then
        $Info "can not find default boot entry in $grubenv."
        return $ERR_NG
    fi


    end_line=$(sed -n '$=' "$menu_lst")
    label_line=$(grep -n "^[[:space:]]*menuentry[[:space:]]\+" $menu_lst | awk -F ":" '{print $1}')
    for line in $label_line
    do
        lable_val=$(sed -n "${line}p" $menu_lst | sed -n '/^menuentry.*$/p'  | sed -n "s/'/\n/gp" | sed -n 2p)
        if [ "${default_entry}" = "${lable_val}" ]
        then
            for ((line_num=$line; line_num<=$end_line; line_num++))
            do
                tmpline=$(sed -n "${line_num}p" $menu_lst)
                if [ -n "$(echo -n "$tmpline" | grep "^[[:space:]]*initrd[[:space:]]\+")" ] ; then initrd_line=$line_num ; return 0; fi
            done
        fi
    done
    return 0
}

boot_match()
{
    if [ -e '/etc/gentoo-release' ]
    then
        return $ERR_OK
    fi
    
    grub_config
    if [ "$SYS" = "grub2" ] && [ -f /etc/redhat-release ]
    then
        if [ ! -f "$GRUBENV" ]
        then
            $Info "can not find $GRUBENV."
            return $ERR_FILE_NOEXIST
        fi
        match=$(cat /boot/grub2/grubenv | grep -w `uname -r`)
        if [ -z "$match" ]
        then
            if [ -f /etc/euleros-release ] && [ "`cat /etc/euleros-release`" == "EulerOS V2.0" ]
            then
                return $ERR_OK
            fi
            $Info "the kernel is $kernel,do not match."
            return $ERR_NG
        fi
        return $ERR_OK
    fi
    if [ "$SYS" = "elilo" ]
    then
        info_boot_entry_uefi "$MENULIST" ""
    elif [ "$SYS" =  "grub2" ]
    then
        # not support openSUSE 13.2
        if [ $(echo -n ${kernel} |grep "3.16.6-2") ]
        then
            return $ERR_OK
        fi
        info_boot_entry_grub2 "$MENULIST" "$GRUBENV"
    else
        info_boot_entry "$MENULIST" ""
    fi
    if [ -z "$initrd_line" ]
    then
        $Info "initrd_line is null."
        return $ERR_NG
    fi
    $Info "initrd_line is $initrd_line."
    line=$(sed -n "${initrd_line}p" $MENULIST)
    $Info "line is $line."
    match=$(echo -n "$line" | grep "$kernel")
    if [ -z "$match" ]
    then
        $Info "the kernel is $kernel,do not match $line."
        return $ERR_NG
    fi
    return $ERR_OK
}
lib_match()
{
    if [ ! -d "/lib/modules/`uname -r`" ]
    then
        $Info "can not find the kernel directory."
        return $ERR_NG
    fi
    return $ERR_OK
}
main()
{
    kernel=$(uname -r)
    if ( lib_match && boot_match )
    then
        return $ERR_OK
    else
        return $ERR_NG
    fi
}
main
