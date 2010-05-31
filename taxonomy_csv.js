// $Id$

if (Drupal.jsEnabled) {
  (function ($) {

    /**
    * Allows to hide unchosen sub-options and to show chosen sub-options.
    *
    * @todo To factorize.
    */
    $(document).ready(function() {
      // Import options
      // Hide all items defined with the css class filtered.
      var methods_import = new Array('#description_alone_terms', '#description_fields_links', '#description_flat', '#description_tree_structure', '#description_polyhierarchy', '#description_parents', '#description_children', '#description_relations', '#description_fields', '#description_descriptions', '#description_weights', '#description_synonyms', '#description_taxonomy_manager', '#existing_items_alone_terms','#existing_items_fields_links', '#existing_items_flat', '#existing_items_tree_structure', '#existing_items_polyhierarchy', '#existing_items_parents', '#existing_items_children', '#existing_items_relations', '#existing_items_fields', '#existing_items_descriptions', '#existing_items_weights', '#existing_items_synonyms', '#existing_items_taxonomy_manager', '#help_alone_terms', '#help_fields_links', '#help_flat', '#help_tree_structure', '#help_polyhierarchy', '#help_parents', '#help_children', '#help_relations', '#help_fields', '#help_descriptions', '#help_weights', '#help_synonyms', '#help_taxonomy_manager', '#edit-relations', '#edit-fields-links');
      for(var m in methods_import) {
        $(methods_import[m]).addClass('filtered');
      }
      // Import format description and existing terms.
      // Add/remove class to show/hide it.
      $('#edit-import-format').change(function(){
        var methods_import_format = new Array('alone_terms', 'fields_links', 'flat', 'tree_structure', 'polyhierarchy', 'parents', 'children', 'relations', 'fields', 'descriptions', 'weights', 'synonyms', 'taxonomy_manager');
        // Remove all added classes in order to return to base.
        for(var m in methods_import) {
          for(var n in methods_import_format) {
            $(methods_import[m]).removeClass(methods_import_format[n]);
          }
        }
        // Add current class to show it with css.
        for(var m in methods_import) {
          $(methods_import[m]).addClass(this.value)
            .animate({opacity:.5}, 1)
            .animate({opacity:1}, 1)
        }
      });
      // Update current display.
      $('#edit-import-format').trigger('change');

      // Import options.
      $('#import').addClass('filtered');
      // Source choice.
      // Add/remove class to show/hide it.
      $('#edit-source-choice').change(function(){
        var methods = new Array('text', 'path', 'url');
        for(var m in methods) {
          $('#import').removeClass(methods[m]);
          $('#edit-utf8-check-wrapper').removeClass(methods[m]);
        }
        $('#import').addClass(this.value)
          .animate({opacity:.5}, 1)
          .animate({opacity:1}, 1)
        $('#edit-utf8-check-wrapper').addClass(this.value);
      });
      // Update current display.
      $('#edit-source-choice').trigger('change');

      // Vocabulary hierarchy.
      // Add/remove class to show/hide it.
      $('#edit-hierarchy-check').change(function(){
        if (this.checked) {
          $('#hierarchy_level').addClass('filtered');
        }
        else {
          $('#hierarchy_level').removeClass('filtered');
        }
      });
      // Update current display.
      $('#edit-hierarchy-check').trigger('change');

      // Import/Export CSV format options.
      $('#csv_format').addClass('filtered');
      // Export delimiter.
      // Add/remove class to show/hide it.
      $('#delimiter').change(function(){
        var methods = new Array('comma', 'semicolon', 'tabulation', 'space', 'currency_sign', 'custom_delimiter');
        for(var m in methods) {
          $('#csv_format').removeClass(methods[m]);
        }
        $('#csv_format').addClass(this.value)
          .animate({opacity:.5}, 1)
          .animate({opacity:1}, 1)
      });
      // Update current display.
      $('#delimiter').trigger('change');

      // Import/Export CSV enclosure.
      // Add/remove class to show/hide it.
      $('#enclosure').change(function(){
        var methods = new Array('none', 'quotation', 'custom_enclosure');
        for(var m in methods) {
          $('#csv_format').removeClass(methods[m]);
        }
        $('#csv_format').addClass(this.value)
          .animate({opacity:.5}, 1)
          .animate({opacity:1}, 1)
      });
      // Update current display.
      $('#enclosure').trigger('change');

      // Destination choice.
      $('#destination').addClass('filtered');
      // Add/remove class to show/hide it.
      $('#edit-vocabulary-target').change(function(){
        var methods = new Array('autocreate', 'duplicate', 'existing');
        for(var m in methods) {
          $('#destination').removeClass(methods[m]);
        }
        $('#destination').addClass(this.value)
          .animate({opacity:.5}, 1)
          .animate({opacity:1}, 1)
      });
      // Update current display.
      $('#edit-vocabulary-target').trigger('change');

      // Result level.
      // Add/remove class to show/hide it.
      $('#edit-result-level').change(function(){
        var methods = new Array('none', 'warnings', 'notices', 'infos');
        for(var m in methods) {
          $('#result_type').removeClass(methods[m]);
        }
        $('#result_type').addClass(this.value);
      });
      // Update current display.
      $('#edit-result-level').trigger('change');

      // Export options (specific)
      // Hide all items defined with the css class filtered.
      var methods_export = new Array('#specific_info', '#edit-fields-links');
      for(var m in methods_export) {
        $(methods_export[m]).addClass('filtered');
      }
      // Add/remove class to show/hide it.
      $('#edit-export-format').change(function(){
        var methods_export_format = new Array('alone_terms', 'fields_links', 'flat', 'tree_structure', 'polyhierarchy', 'parents', 'children', 'relations', 'fields', 'descriptions', 'weights', 'synonyms', 'taxonomy_manager');
        // Remove all added classes in order to return to base.
        for(var m in methods_export) {
          for(var n in methods_export_format) {
            $(methods_export[m]).removeClass(methods_export_format[n]);
          }
        }
        // Add current class to show it with css.
        for(var m in methods_export) {
          $(methods_export[m]).addClass(this.value)
            .animate({opacity:.5}, 1)
            .animate({opacity:1}, 1)
        }
      });
      // Update current display.
      $('#edit-export-format').trigger('change');
    });

  })(jQuery);
}
