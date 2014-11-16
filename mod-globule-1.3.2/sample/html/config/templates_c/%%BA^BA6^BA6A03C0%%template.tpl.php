<?php /* Smarty version 2.6.12, created on 2006-03-10 16:18:21
         compiled from template.tpl */ ?>
<?php require_once(SMARTY_CORE_DIR . 'core.load_plugins.php');
smarty_core_load_plugins(array('plugins' => array(array('block', 'php_supported', 'template.tpl', 14, false),array('function', 'php_unsupported', 'template.tpl', 19, false),)), $this); ?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
        "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html><head>
  <title>Globule Broker System</title>
  <meta http-equiv="Content-Type" content="text/html; charset=ISO-8859-1" />
  <link rel="stylesheet" href="gbs.css" type="text/css" />
  <link rel="SHORTCUT ICON" href="http://www.globule.org/favicon.ico">
</head><body>
  <div id="topImage">
    <img src="images/globsm.jpg" alt="GBS logo" align="top"/><span class="Logo">Globule Broker System</span><span class="Title">: <?php echo $this->_tpl_vars['title']; ?>
</span>
  </div>
  <div id="userName">
    <div id="loginStatus">
<?php $this->_tag_stack[] = array('php_supported', array()); $_block_repeat=true;smarty_block_php_supported($this->_tag_stack[count($this->_tag_stack)-1][1], null, $this, $_block_repeat);while ($_block_repeat) { ob_start(); ?>
      <form>
        <input type="text" name="username" value=""><br>
        <input type="password" name="password" value="">
      </form>
<?php echo smarty_function_php_unsupported(array(), $this);?>

      no login available
<?php $_block_content = ob_get_contents(); ob_end_clean(); $_block_repeat=false;echo smarty_block_php_supported($this->_tag_stack[count($this->_tag_stack)-1][1], $_block_content, $this, $_block_repeat); }  array_pop($this->_tag_stack); ?>
    </div>
    <?php echo $this->_tpl_vars['header']; ?>

  </div>
  <div id="leftOptions">
<?php $this->_tag_stack[] = array('php_supported', array()); $_block_repeat=true;smarty_block_php_supported($this->_tag_stack[count($this->_tag_stack)-1][1], null, $this, $_block_repeat);while ($_block_repeat) { ob_start(); ?>
    <a href="./">index</a><br>
<?php echo smarty_function_php_unsupported(array(), $this);?>

    &nbsp;
<?php $_block_content = ob_get_contents(); ob_end_clean(); $_block_repeat=false;echo smarty_block_php_supported($this->_tag_stack[count($this->_tag_stack)-1][1], $_block_content, $this, $_block_repeat); }  array_pop($this->_tag_stack); ?>
  </div>
  <div id="mainPage">
    <?php echo $this->_tpl_vars['content']; ?>

  </div>
  <div id="footer"><address>berry@cs.vu.nl</address></div>
</body></html>