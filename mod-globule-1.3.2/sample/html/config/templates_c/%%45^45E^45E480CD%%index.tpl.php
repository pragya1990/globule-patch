<?php /* Smarty version 2.6.12, created on 2006-03-10 16:19:51
         compiled from index.tpl */ ?>
<?php require_once(SMARTY_CORE_DIR . 'core.load_plugins.php');
smarty_core_load_plugins(array('plugins' => array(array('block', 'page', 'index.tpl', 1, false),array('block', 'pagecontent', 'index.tpl', 4, false),)), $this); ?>
<?php $this->_tag_stack[] = array('page', array('template' => 'template')); $_block_repeat=true;smarty_block_page($this->_tag_stack[count($this->_tag_stack)-1][1], null, $this, $_block_repeat);while ($_block_repeat) { ob_start(); ?>
  <?php $this->assign('title', 'First Time'); ?>
  <?php $this->assign('header', 'Welcome');  $this->_tag_stack[] = array('pagecontent', array()); $_block_repeat=true;smarty_block_pagecontent($this->_tag_stack[count($this->_tag_stack)-1][1], null, $this, $_block_repeat);while ($_block_repeat) { ob_start(); ?>

Deze pagina's helpen je je fantastische Globule server te onderhouden.
Zonder het eerste onderhoud is je web-server nog vrij nutteloos.  Immers,
zonder web-pagina's en een web-adres waar je server benaderd kan worden kan
niemand iets zien.
Het eerste wat nodig is, is een web-adres voor je shiny new server. Als
anderen op het Internet dit adres in hun browser intikken, komen jouw
pagina's via jouw, en andere computers afgeleverd.

<p/><spacer type="block" height="300" width="1">
<?php $_block_content = ob_get_contents(); ob_end_clean(); $_block_repeat=false;echo smarty_block_pagecontent($this->_tag_stack[count($this->_tag_stack)-1][1], $_block_content, $this, $_block_repeat); }  array_pop($this->_tag_stack);  $_block_content = ob_get_contents(); ob_end_clean(); $_block_repeat=false;echo smarty_block_page($this->_tag_stack[count($this->_tag_stack)-1][1], $_block_content, $this, $_block_repeat); }  array_pop($this->_tag_stack); ?>