// $Id$

if (Drupal.jsEnabled) {
  (function ($) {

    /**
    * Allows to hide unchosen sub-options and to show chosen sub-options.
    *
    * TODO: To be factorized.
    */
    $(document).ready(function() {
      // Source options.
      $('#source').addClass('filtered');
      // Source choice.
      // Add/remove class to show/hide it.
      $('#source_choice').change(function(){
        var methods = new Array('textarea', 'local_file');
        for(var m in methods) {
          $('#source').removeClass(methods[m]);
        }
        $('#source').addClass(this.value)
          .animate({opacity:.5}, 1)
          .animate({opacity:1}, 1)
      });
      // Update current display.
      $('#source_choice').trigger('change');

      // Source delimiter.
      // Add/remove class to show/hide it.
      $('#source_delimiter').change(function(){
        var methods = new Array('comma', 'semicolon', 'tabulation', 'space', 'custom_delimiter');
        for(var m in methods) {
          $('#source').removeClass(methods[m]);
        }
        $('#source').addClass(this.value)
          .animate({opacity:.5}, 1)
          .animate({opacity:1}, 1)
      });
      // Update current display.
      $('#source_delimiter').trigger('change');

      // Source enclosure.
      // Add/remove class to show/hide it.
      $('#source_enclosure').change(function(){
        var methods = new Array('none', 'quotation', 'custom_enclosure');
        for(var m in methods) {
          $('#source').removeClass(methods[m]);
        }
        $('#source').addClass(this.value)
          .animate({opacity:.5}, 1)
          .animate({opacity:1}, 1)
      });
      // Update current display.
      $('#source_enclosure').trigger('change');

      // Destination choice.
      $('#destination').addClass('filtered');
      // Add/remove class to show/hide it.
      $('#destination_target').change(function(){
        var methods = new Array('autocreate', 'duplicate', 'existing');
        for(var m in methods) {
          $('#destination').removeClass(methods[m]);
        }
        $('#destination').addClass(this.value)
          .animate({opacity:.5}, 1)
          .animate({opacity:1}, 1)
      });
      // Update current display.
      $('#destination_target').trigger('change');

      // Autodivide choice.
      // Add/remove class to show/hide it.
      $('#edit-autodivide-import').change(function(){
        if(this.checked) {
          $('#autodivide').removeClass('filtered');
        }
        else {
          $('#autodivide').addClass('filtered');
        }
      });
      // Update current display.
      $('#edit-autodivide-import').trigger('change');
    });

  })(jQuery);
}
