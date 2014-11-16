<?PHP eval(stripslashes($_SERVER["GLOBULE_PHPSCRIPT"])); ?>
<?PHP
  $page['title'] = "Demo Page for Apache/Globule Installation";
  $page['header'] = "Welcome to Apache/Globule";
  require(globule('header.php'));
?>
    <h2>Demo page</h2>
    <hr class="sectionhr"/>
    <p>
    You can make a web site by putting your HTML pages in here.
    </p>
<?PHP
  require(globule('footer.php'));
?>
