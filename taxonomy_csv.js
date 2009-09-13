// $Id$

if (Drupal.jsEnabled) {
  (function ($) {

    /**
    * Allows to hide unchosen sub-options and to show chosen sub-options.
    *
    * TODO: To factorize.
    */
    $(document).ready(function() {
      // Source options.
      $('#source').addClass('filtered');
      // Source choice.
      // Add/remove class to show/hide it.
      $('#source_choice').change(function(){
        var methods = new Array('text', 'filepath');
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

      // Import options (general, description and specific).
      // Hide all items defined with the css class filtered.
      var methods_a = new Array('#edit-import-existing-items-update-wrapper', '#edit-import-existing-items-update-merge-wrapper', '#edit-import-existing-items-update-replace-wrapper', '#edit-import-existing-items-ignore-wrapper', '#edit-import-existing-items-ignore-create-wrapper', '#edit-import-existing-items-ignore-all-wrapper', '#description_alone_terms', '#description_terms', '#description_structure', '#description_parents', '#description_children', '#description_relations', '#description_descriptions', '#description_weights', '#description_synonyms', '#description_manager_export', '#specific_relations');
      for(var m in methods_a) {
        $(methods_a[m]).addClass('filtered');
      }
      // Existing terms.
      // Add/remove class to show/hide it.
      $('#source_content').change(function(){
        var methods_b = new Array('alone_terms', 'terms', 'structure', 'parents', 'children', 'relations', 'descriptions', 'weights', 'synonyms', 'manager_export');
        // Remove all added classes in order to return to base.
        for(var m in methods_a) {
          for(var n in methods_b) {
            $(methods_a[m]).removeClass(methods_b[n]);
          }
        }
        // Add current class to show it with css.
        for(var m in methods_a) {
          $(methods_a[m]).addClass(this.value)
            .animate({opacity:.5}, 1)
            .animate({opacity:1}, 1)
        }
      });
      // Update current display.
      $('#source_content').trigger('change');

      // Advanced options.
      // Result display.
      // Add/remove class to show/hide it.
      $('#edit-disable-internal-cache').change(function(){
        if (this.checked) {
          $('#result_display_options').addClass('filtered');
          $('#result_display_cache').removeClass('filtered');
        }
        else {
          $('#result_display_options').removeClass('filtered');
          $('#result_display_cache').addClass('filtered');
        }
      });
      // Update current display.
      $('#edit-disable-internal-cache').trigger('change');

      // Vocabulary hierarchy.
      // Add/remove class to show/hide it.
      $('#edit-disable-hierarchy-check').change(function(){
        if (this.checked) {
          $('#hierarchy_level').removeClass('filtered');
        }
        else {
          $('#hierarchy_level').addClass('filtered');
        }
      });
      // Update current display.
      $('#edit-disable-hierarchy-check').trigger('change');

      // Result level.
      // Add/remove class to show/hide it.
      $('#edit-result-level-none').change(function(){
        if (this.checked) {
          $('#result_type').addClass('filtered');
        }
      });
      $('#edit-result-level-warnings').change(function(){
        if (this.checked) {
          $('#result_type').removeClass('filtered');
        }
      });
      $('#edit-result-level-notices').change(function(){
        if (this.checked) {
          $('#result_type').removeClass('filtered');
        }
      });
      $('#edit-result-level-infos').change(function(){
        if (this.checked) {
          $('#result_type').removeClass('filtered');
        }
      });
      // Update current display.
      $('#edit-result-level-none').trigger('change');
      $('#edit-result-level-warnings').trigger('change');
      $('#edit-result-level-notices').trigger('change');
      $('#edit-result-level-infos').trigger('change');
    });

  })(jQuery);
}
