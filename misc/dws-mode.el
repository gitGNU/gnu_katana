;;; dws-mode.el --- Katana Dwarfscript editing mode for Emacs
;; Author: James Oakley

(defgroup dws nil
  "Major mode for editing Katana DWS files"
  :group 'tools)

(defcustom dws-indent-width default-tab-width
  "*Indentation width in Katana DWS mode buffers."
  :type 'integer
  :group 'dws)

(defconst dws-keywords
  '("begin" "end" "false")
  "Dwarfscript Keywords")

(defconst dws-types
  '("CIE" "cie" "FDE" "fde" "EXPRESSION" "expression" "INSTRUCTIONS" 
    "instructions" "LSDA" "lsda" "CALL_SITE" "call_site" "ACTION" "action")
  "Dwarfscript Types")

(defconst dws-builtins
  '("DW_EH_PE_sdata4" "DW_EH_PE_udata4" "DW_EH_PE_sdata4" "DW_EH_PE_udata4" 
    "DW_EH_PE_sdata8" "DW_EH_PE_udata8" "DW_EH_PE_pcrel" "DW_EH_PE_textrel"
    "DW_EH_PE_datarel" "DW_EH_PE_funcrel" "DW_EH_PE_aligned" "DW_EH_PE_omit"
    "DW_EH_PE_absptr" "DW_EH_PE_uleb128" "DW_EH_PE_sleb128")
  "Dwarfscript Symbolic Constants")

;;these are property names
(defconst dws-variables
  '("section_type" "eh_hdr_addr" "section_addr" "except_table_addr" "index"
    "version" "data_align" "code_align" "return_addr_rule" "fde_ptr_enc" 
    "fde_lsda_ptr_enc" "personality_ptr_enc" "personality" "cie_index" 
    "initial_location" "address_range" "lsda_idx" "position" "length" 
    "landing_pad" "has_action" "lpstart" "typeinfo_enc" "first_action"
    "next" "typeinfo" "type_idx" "eh_hdr_table_enc")
  
  "Dwarfscript Properties")



(defconst dws-font-lock-keywords
    (list
     (cons (regexp-opt dws-keywords 'words) 'font-lock-keyword-face)
     (cons (regexp-opt dws-types 'words) 'font-lock-type-face)
     ;;these are property names
     (cons (regexp-opt dws-variables 'words) 'font-lock-variable-name-face)
     ;;symbolic constants
     (cons (regexp-opt dws-builtins 'words) 'font-lock-builtin-face)
                                        ; DWARF instruction names
     (cons "\\(?:DW_CFA\\|DW_OP\\)_[a-zA-Z0-9_]*" 'font-lock-function-name-face)))

(define-derived-mode dws-mode fundamental-mode "DWS"
  "Major mode for editing Katana dwarfscript files"
  (setq font-lock-defaults '(dws-font-lock-keywords nil nil ((?_ . "w"))))
  (modify-syntax-entry ?# "< b" dws-mode-syntax-table)
  (modify-syntax-entry ?\n "> b" dws-mode-syntax-table)
  (set (make-local-variable 'indent-line-function) 'dws-indent-line)
  (setq comment-start "# "))


(defun dws-indent-line ()
  "Indent current line of dws code."
  (interactive)
  (indent-line-to
   (save-excursion
     (move-to-column (current-indentation))
     (if (looking-at "end\\W")
         (progn
           (forward-line -1)
           (max 0 (- (current-indentation) dws-indent-width)))
       (progn
         (forward-line -1)
         (move-to-column (current-indentation))
         (if (looking-at "begin\\W")
             (max 0 (+ (current-indentation) dws-indent-width))
           (current-indentation)))))))
