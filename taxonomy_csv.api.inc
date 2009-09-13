<?php
// $Id$

/**
 * @file
 * This API allows to use taxonomy_csv import functions from any other module.
 *
 * Functions sets:
 * 1. Prepare and import a vocabulary : taxonomy_csv_vocabulary_import
 * 2. Prepare and import a line       : taxonomy_csv_line_import
 * 3. Prepare and import a term       : taxonomy_csv_term_import
 * 4. Errors helpers
 * 5. Info and log messages
 *
 *
 * Structure of api:
 * -  1. Batch prepare import of file or text
 * -  2. Process import line by line (see below)
 * -  3. Evaluate vocabulary and finish process
 *
 * Process import structure (line by line import from a batch set):
 * - 1. Validate line
 *     1. Clean input line
 *     2. Check line items
 * - 2. Prepare to process items matching import type (eventual loop)
 * - 3. Process import
 *     1. Find previous or existing term (see below)
 *     2. Update or create term
 *     3. Cache term
 *
 * Find a term before update or create it:
 * - 1. In previous imported
 *     1. In parent if structure
 *     2. In whole cache in all cases
 *     3. In extended cache if related (internal use only)
 * - 2. In existing terms of the vocabulary (except ignore all)
 *     1. In parent if structure
 *     2. In whole vocabulary in all cases
 *     3. In all vocabularies if related (internal use only)
 *
 *
 * To add a new csv scheme, need:
 * - a define without space,
 * - items in _taxonomy_csv_lists,
 * - items in taxonomy_csv.js and taxonomy_csv.css,
 * - a description in forms (taxonomy_csv.module),
 * - an advanced help.
 * - a case in _taxonomy_csv_check_items(),
 * - a case in taxonomy_csv_import_line_items(),
 * - eventually specific options.
 */

define('TAXONOMY_CSV_SOURCE_ALONE_TERMS',       'alone_terms');
define('TAXONOMY_CSV_SOURCE_TERMS',             'terms');
define('TAXONOMY_CSV_SOURCE_STRUCTURE',         'structure');
define('TAXONOMY_CSV_SOURCE_PARENTS',           'parents');
define('TAXONOMY_CSV_SOURCE_CHILDREN',          'children');
define('TAXONOMY_CSV_SOURCE_RELATIONS',         'relations');
define('TAXONOMY_CSV_SOURCE_DESCRIPTIONS',      'descriptions');
define('TAXONOMY_CSV_SOURCE_WEIGHTS',           'weights');
define('TAXONOMY_CSV_SOURCE_SYNONYMS',          'synonyms');
define('TAXONOMY_CSV_SOURCE_MANAGER_EXPORT',    'manager_export');

define('TAXONOMY_CSV_EXISTING_UPDATE',          'update'); // When no difference between merge and replace.
define('TAXONOMY_CSV_EXISTING_UPDATE_MERGE',    'update_merge');
define('TAXONOMY_CSV_EXISTING_UPDATE_REPLACE',  'update_replace');
define('TAXONOMY_CSV_EXISTING_IGNORE',          'ignore'); // When no difference between create and all. Equal to duplicate.
define('TAXONOMY_CSV_EXISTING_IGNORE_CREATE',   'ignore_create');
define('TAXONOMY_CSV_EXISTING_IGNORE_ALL',      'ignore_all'); // Ignore even existing terms in additional columns.
define('TAXONOMY_CSV_EXISTING_IGNORE_PREVIOUS', 'ignore_previous'); // Internal, for previous parents. To be removed.

define('TAXONOMY_CSV_WATCHDOG_ERROR',           3); // Stop import process.
define('TAXONOMY_CSV_WATCHDOG_WARNING',         4); // Stop line process and go to next one.
define('TAXONOMY_CSV_WATCHDOG_NOTICE',          5); // Continue current line process.
define('TAXONOMY_CSV_WATCHDOG_INFO',            6); // Successfully processed.
define('TAXONOMY_CSV_WATCHDOG_DEBUG',           7); // Internal use only.
define('TAXONOMY_CSV_WATCHDOG_NONE',            9); // Internal use only.

// Information about import process. Use too SAVED_NEW = 1 and SAVED_UPDATED = 2. Possibly use of SAVED_DELETED = 3.
define('TAXONOMY_CSV_ERROR',                    0);
define('TAXONOMY_CSV_NEW_UPDATED',              4);

/**
 * @defgroup taxonomy_csv_vocabulary Import a vocabulary
 * @{
 * Functions allowing import of a vocabulary into a new or an existing one.
 */

/**
 * Prepare the import of a vocabulary.
 * If not used in a form, don't forget to use batch_process().
 *
 * @param $options
 *   Array of options:
 *     source_choice  : 'filepath' or 'text'
 *     filepath       : url or path of csv file
 *     file           : object file if file is already uploaded
 *     text           : csv text to import
 *     source_content : see _taxonomy_csv_lists('list_source_content_types')
 *     delimiter      : one character csv delimiter (default: ",")
 *     enclosure      : zero or one character csv enclosure (default: none, i.e. '"')
 *     destination_target           : 'autocreate' (default), 'duplicate' or 'existing'
 *     destination_vocabulary_id    : vocabulary id to duplicate or to import into
 *     import_existing_items        : see _taxonomy_csv_lists('list_import_options')
 *     relations_create_subrelations: boolean. specific option for related terms import
 *     relations_all_vocabularies   : boolean. specific option for related terms import
 *     internal_cache : boolean. tweak to use (default) or not internal cache
 *     hierarchy_check: boolean. tweak to check (default) or not hierarchy of vocabulary
 *     hierarchy_level: if hierarchy_check is false, set hierarchy level (0, 1 or 2)
 *     line_checks    : boolean. tweak to check (default) or not format of lines
 *     utf8_check     : boolean. tweak to check (default) or not utf8 format
 *     result_stats   : boolean. Display (default) or not stats
 *     result_terms   : boolean. Display (default) or not list of imported terms
 *     result_level   : Log level: 'none', 'warnings' (default), 'notices' or 'infos'
 *     result_type    : Display of log: 'by_message' (default) or 'by_line'
 *   Needed options are source_choice, filepath or text, source_content and import_existing_items.
 * @return
 *   Array of errors or nothing (batch process to execute).
 */
function taxonomy_csv_vocabulary_import($options) {
  // Check and eventually update options.
  $result = _taxonomy_csv_vocabulary_import_check_options($options);
  if (count($result)) {
    return $result;
  }

  // Prepare import by text: save text as a temp file to simplify process and avoid memory congestion.
  if ($options['source_choice'] == 'text') {
    $filename = file_save_data(
      $options['text'],
      tempnam(realpath(file_directory_temp()), 'taxo_csv_'),
      'FILE_EXISTS_RENAME');
    if (!$filename) {
      return array('text' => t('Import by text needs access to temp directory. Import failed.'));
    }
    $options['file'] = new stdClass();
    $options['file']->filename = basename($filename);
    $options['file']->filepath = $filename;
    $options['file']->filesize = filesize($filename);
    $options['text'] = '';
    $options['utf8_check'] = FALSE;
  }

  // User choose to autocreate or duplicate a vocabulary.
  switch ($options['destination_target']) {
    case 'autocreate':
      $new_vocabulary = taxonomy_csv_vocabulary_create(($options['source_choice'] == 'filepath') ? $options['file']->filename : '');
      $options['destination_vocabulary_id'] = $new_vocabulary['vid'];
      break;

    case 'duplicate':
      $new_vocabulary = taxonomy_csv_vocabulary_duplicate($options['destination_vocabulary_id']);
      $options['destination_vocabulary_id'] = $new_vocabulary['vid'];
      break;
  }
  $options['vocabulary'] = taxonomy_vocabulary_load($options['destination_vocabulary_id']);

  // Calculates number of lines to be imported.
  // Automatically detects line endings.
  ini_set('auto_detect_line_endings', '1');
  $handle = fopen($options['file']->filepath, 'r');
  $lines_count = 0;
  while ($line = fgetcsv($handle, 32768, $options['delimiter'], $options['enclosure'])) {
    $lines_count++;
  }
  fclose($handle);
  $options['total_lines'] = $lines_count;

  // Prepare import batch.
  $batch = array(
    'title'            => ($options['source_choice'] == 'filepath') ?
        t('Importing terms from CSV file "%filename"...', array('%filename' => $options['file']->filename)) :
        t('Importing terms from text...'),
    'init_message'     => t('Starting uploading of datas...'),
    'progress_message' => '',
    'error_message'    => t('An error occurred during the import.'),
    'finished'         => '_taxonomy_csv_vocabulary_import_finished',
    'file'             => dirname(drupal_get_filename('module', 'taxonomy_csv')) .'/taxonomy_csv.api.inc',
    'progressive'      => TRUE,
    'operations'       => array(
        0 => array('_taxonomy_csv_vocabulary_import_process', array($options))
    ),
  );

  batch_set($batch);
}

/**
 * Validate options of imported vocabulary or line.
 *
 * @param $options
 *   Array of options.
 * @return
 *   Array of messages errors if any.
 *   By reference options are cleaned and completed.
 */
function _taxonomy_csv_vocabulary_import_check_options(&$options) {
  $messages = array();

  // Set default value for unset options.
  foreach (array(
      'delimiter'          => ',',
      'enclosure'          => '',
      'destination_target' => 'autocreate',
      'internal_cache'     => TRUE,
      'hierarchy_check'    => TRUE,
      'line_checks'        => TRUE,
      'utf8_check'         => TRUE,
      'result_stats'       => 'result_stats',
      'result_terms'       => 'result_terms',
      'result_level'       => 'notices',
      'result_type'        => 'by_message',
    ) as $key => $value) {
    if (!isset($options[$key])) {
      $options[$key] = $value;
    }
  }

  switch ($options['source_choice']) {
    case 'filepath':
      // Upload if not already uploaded.
      if (!isset($options['file'])) {
        $options['file'] = file_save_upload($options['filepath']);
      }
      if (!$options['file']) {
        $messages['filepath'] = t("You choose to import a taxonomy by a file, but you don't set its name or its size is greater than the server's limit of !max_size.", array('!max_size' => format_size(parse_size(ini_get('upload_max_filesize')))));
      }
      else if (!$options['file']->filesize) {
        $messages['filepath'] = t('Size of your file is null.');
      }
      else {
        //  No error, so clean textarea in order to decrease usage of memory.
        $options['text'] = '';
      }
      break;

    case 'text':
      if (empty($options['text'])) {
        $messages['text'] = t('You choose to import a taxonomy by a text, but the text is empty.');
      }
      break;

    default:
      $messages['source_choice'] = t('Source choice is "filepath" or "text".');
  }

  // Delimiter and enclosure greater than one character are useless with fgetcsv.
  if (drupal_strlen($options['delimiter']) != 1) {
    $messages['delimiter'] = t('Delimiter should be a one character string.');
  }
  if (drupal_strlen($options['enclosure']) == 0) {
    // With fgetcsv, $enclosure == '' bugs, so use default quote enclosure if none.
    $options['enclosure'] = '"';
  }
  else if (drupal_strlen($options['enclosure']) > 1) {
    $messages['enclosure'] = t('Enclosure lenght cannot be greater than one character.');
  }

  if ($options['destination_target'] != 'autocreate'
      && $options['destination_target'] != 'duplicate'
      && $options['destination_target'] != 'existing') {
    $messages['destination_target'] = t('Destination target should be "autocreate", "duplicate" or "existing".');
  }
  if ($options['destination_target'] == 'duplicate' || $options['destination_target'] == 'existing') {
    $list_vocabularies = taxonomy_get_vocabularies();
    if (!isset($list_vocabularies[$options['destination_vocabulary_id']])) {
      $messages['destination_vocabulary_id'] = t("You choose to use an existing vocabulary, but you haven't choose it.");
    }
  }

  $list_source_content_types = _taxonomy_csv_lists('list_source_content_types');
  $list_import_options = _taxonomy_csv_lists('list_import_options');
  $list_source_content_allowed_import_options = _taxonomy_csv_lists('list_source_content_allowed_import_options');

  if (!array_key_exists($options['source_content'], _taxonomy_csv_lists('list_source_content_types'))) {
    $messages['source_content'] = t('Source content "%source_content" is not managed.', array('%source_content' => $list_source_content_types[$options['source_content']]));
  }
  else {
    if (!in_array($options['import_existing_items'], $list_source_content_allowed_import_options[$options['source_content']])) {
      $messages['import_existing_items'] = t('Import option "%import_existing_items" cannot be used with source content "%source_content".', array(
        '%import_existing_items' => $list_import_options[$options['import_existing_items']],
        '%source_content'        => $list_source_content_types[$options['source_content']],
      ));
    }
  }

  if (!$options['import_existing_items']) {
    $messages['import_existing_items'] = t('Please set what will become existing terms.');
  }
  else if (!array_key_exists($options['import_existing_items'], $list_import_options)) {
    $messages['import_existing_items'] = t('Import option "%import_existing_items" is not managed.', array('%import_existing_items' => $list_import_options[$options['import_existing_items']]));
  }

  if ($options['hierarchy_check']
      && ($options['hierarchy_level'] < 0 || $options['hierarchy_level'] > 2)) {
    $messages['hierarchy_level'] = t('You need to set hierarchy level if hierarchy check of vocabulary is disabled.');
  }

  // Advertise if Pathauto module is activated. Issue http://drupal.org/node/540916.
  if (module_exists('pathauto')) {
    $messages['pathauto'] = t("<strong>Warning</strong>: Pathauto module is activated.<br />This module slows down taxonomy import process and only few terms can be imported. It's advised to disabled it manually in !modules_section. Settings aren't lost when you disable it - and not uninstall it -.<br />After import process, you can reactivate Pathauto and eventually bulk generate aliases for newly imported terms.", array('!modules_section' => l(t('modules section'), 'admin/build/modules')));
  }

  return $messages;
}

/**
 * Batch process of vocabulary import.
 *
 * @param $options
 *   Array of options.
 * @param &$context
 *   Batch context to keep results and messages.
 * @return
 *   NULL because use of &$context.
 */
function _taxonomy_csv_vocabulary_import_process($options, &$context) {
  // First callback.
  if (empty($context['sandbox'])) {
    // Remember options as $batch_set can't use $form_storage.
    // It allows too that first line in result is numbered 1 and not 0.
    $context['results'][0] = $options;

    // Initialize some variables.
    $context['results'][0]['current_line'] = 0;
    $context['results'][0]['worst_line'] = 0;
    $context['results'][0]['worst_message'] = 999;
    $context['results'][0]['handle'] = fopen($options['file']->filepath, 'r');
    $context['sandbox']['handle_pointer'] = 0;
    $context['sandbox']['max'] = $options['total_lines'];
    $context['sandbox']['current'] = &$context['results'][0]['current_line'];
    $context['sandbox']['previous_line'] = array(
      'name' => array(),
      'tid'  => array(),
    );

    $first = TRUE;

    // Automatically detect line endings.
    ini_set('auto_detect_line_endings', '1');

    // Enable or disable cache.
    _taxonomy_csv_term_get_previous_imported($options['internal_cache'], 'use_cache');
  }
  else if (!is_resource($context['results'][0]['handle'])) {
    // Recall file and set pointer in case of memory or time out.
    $context['results'][0]['handle'] = fopen($options['file']->filepath, 'r');
    fseek($context['results'][0]['handle'], $context['sandbox']['handle_pointer']);

    // Reload internal static cache if it has been removed in case of long batch.
    if ($options['internal_cache']
        && isset($context['results'][0]['imported_terms'])
        && (_taxonomy_csv_term_get_previous_imported('', 'count') == 0)) {
      _taxonomy_csv_term_get_previous_imported($context['results'][0]['imported_terms'], 'set_cache');
    }
  }

  // Load and process one line.
  $worst_line    = &$context['results'][0]['worst_line'];
  $worst_message = &$context['results'][0]['worst_message'];
  $handle        = &$context['results'][0]['handle'];
  $line_number   = &$context['sandbox']['current'];
  $previous_line = &$context['sandbox']['previous_line'];

  if ($line = fgetcsv($handle, 32768, $options['delimiter'], $options['enclosure'])) {
    $line_number++;

    // Remember pointer in case of memory or time out.
    $context['sandbox']['handle_pointer']  = ftell($handle);

    // Skip eventual UTF-8 byte order mark.
    if ($first) {
      if (strncmp($line[0], "\xEF\xBB\xBF", 3) === 0) {
        $line[0] = substr($line[0], 3);
        $first = FALSE;
      }
    }

    // Process import of current line.
    $result = taxonomy_csv_line_import($line, $options, $previous_line);

    // Remember processed line.
    $previous_line['name'] = $result['name'];
    $previous_line['tid']  = $result['tid'];

    // Remember worst message of imported lines.
    $worst_message_new = _taxonomy_csv_messages_get_worst_message($result['msg']);
    if ($worst_message_new < $worst_message) {
      $worst_message = $worst_message_new;
      $worst_line    = $line_number;
    };

    if ($options['internal_cache']) {
      // Remember only wanted logs.
      switch ($options['result_level']) {
        case 'none':
          break;
        case 'warnings':
          $list_messages = array();
          foreach ($result['msg'] as $value) {
            foreach ($value as $msg) {
              if (_taxonomy_csv_messages_get_level($msg) <= TAXONOMY_CSV_WATCHDOG_WARNING) {
                $list_messages[$msg] = $msg;
              }
            }
          }
          if (count($list_messages)) {
            sort($list_messages);
            $context['results'][$line_number] = $list_messages;
          }
          break;
        case 'notices':
          $list_messages = array();
          foreach ($result['msg'] as $value) {
            foreach ($value as $msg) {
              if (_taxonomy_csv_messages_get_level($msg) <= TAXONOMY_CSV_WATCHDOG_NOTICE) {
                $list_messages[$msg] = $msg;
              }
            }
          }
          sort($list_messages);
          $context['results'][$line_number] = $list_messages;
          break;
        case 'infos':
          $list_messages = array();
          foreach ($result['msg'] as $value) {
            foreach ($value as $msg) {
              $list_messages[$msg] = $msg;
            }
          }
          sort($list_messages);
          $context['results'][$line_number] = $list_messages;
          break;
        case 'full':
          $context['results'][$line_number] = $result;
          break;
      }

      // Save static cache to keep informations about process in case of batch timeout.
      $context['results'][0]['imported_terms'] =& _taxonomy_csv_term_get_previous_imported('', 'dump');
    }

    // Inform about progress.
    $context['message'] = t('Line %line_number of %total_lines processed: %line', array(
      '%line_number' => $line_number,
      '%total_lines' => $options['total_lines'],
      // $previous_line['name'] can be used, except with taxonomy_manager_export format.
      '%line'        => implode(', ', $line),
    ));

    // Check worst message of imported lines and update progress.
    if (_taxonomy_csv_messages_get_level($worst_message) > TAXONOMY_CSV_WATCHDOG_ERROR) {
      $context['finished'] = $context['sandbox']['current'] / $context['sandbox']['max'];
    }
    else {
      $context['finished'] = 1;
      if ($options['internal_cache']) _taxonomy_csv_term_get_previous_imported('', 'clear');
    }
  }
}

/**
 * Callback for finished batch import and display result informations.
 */
function _taxonomy_csv_vocabulary_import_finished($success, $results, $operations) {
  // $results[0] is used to save options and some infos (imported terms), as batch process can't use $form_state.
  $options = &$results[0];
  unset($results[0]);

  // Close imported file.
  if ($options['handle']) fclose($options['handle']);

  // Clear internal cache._taxonomy_csv_term_get_previous_imported
  if ($options['internal_cache']) _taxonomy_csv_term_get_previous_imported('', 'clear');

  // Check and update hierarchy.
  $options['new_hierarchy'] = ($options['hierarchy_check']) ? taxonomy_csv_vocabulary_check_hierarchy($options['destination_vocabulary_id']) : taxonomy_csv_vocabulary_check_hierarchy($options['destination_vocabulary_id'], $options['hierarchy_level']);

  // Display general info.
  drupal_set_message(_taxonomy_csv_info_chosen_options($options), 'status');

  // Display general info about vocabulary.
  $message = _taxonomy_csv_info_vocabulary_destination($options['vocabulary'], $options['destination_target']);
  $message .= _taxonomy_csv_info_vocabulary_result($options['vocabulary'], $options['destination_target'], $options['new_hierarchy']);
  if (!$options['hierarchy_check']) $message .= '<br />'. t('Hierarchy level has been manually set.');
  drupal_set_message($message .'<br />', 'status');

  // Prepare batch result message.
  if (!$success) {
    $message = t('Importation failed');
    $message .= '<br />'. t('This issue is related to import process or to size import and probably not to content.');
    if ($options['current_line'] == $options['total_lines']) {
      $message .= '<br />'. t('Import process was successful, but the evaluation of hierarchy level has failed, because it is highly memory consumming. You need to change its value with default Drupal functions or set it in Tweaks section of Taxonomy csv.') .' '. t('This problem can be caused too by a high log level. You can reduce it in advanced options.');
    }
    else {
      $message .= '<br />'. t('Import process was successful until the line %line_count of a total of %total_lines. You can first check your file on this line and check file uploading.', array(
        '%line_count'  => $options['current_line'],
        '%total_lines' => $options['total_lines'],
      ));
    }
    $message .= '<br />'. t('You can reduce log level or disable internal cache to avoid this error.');
    $message .= '<br />'. t('You can reinstall module from a fresh release or submit an issue on <a href="!link">Taxonomy CSV import module</a>.', array(
      '!link' => url('http://drupal.org/project/issues/taxonomy_csv/'),
    ));

    drupal_set_message($message, 'error');
  }
  else {
    // Short summary information.
    switch (_taxonomy_csv_messages_get_level($options['worst_message'])) {
      case TAXONOMY_CSV_WATCHDOG_ERROR:
        drupal_set_message(t('Errors have been reported during import process. Process failed at line %line_number with error: %message', array(
          '%line_number' => $options['worst_line'],
          '%message'     => _taxonomy_csv_info_result_text($options['worst_message']),
        )), 'error');
        break;
      case TAXONOMY_CSV_WATCHDOG_WARNING:
        drupal_set_message(t('Warnings have been reported during import process (bad formatted lines). First line skipped is line %line_number with message: %message', array(
          '%line_number' => $options['worst_line'],
          '%message'     => _taxonomy_csv_info_result_text($options['worst_message']),
        )), 'error');
        break;
      case TAXONOMY_CSV_WATCHDOG_NOTICE:
        drupal_set_message(t('Notices have been reported during import process (bad formatted or empty lines). Lines processed. First notice on line %line_number with notice: %message', array(
          '%line_number' => $options['worst_line'],
          '%message'     => _taxonomy_csv_info_result_text($options['worst_message']),
        )), 'warning');
        break;
      case TAXONOMY_CSV_WATCHDOG_INFO:
      default:
        drupal_set_message(t('No error, warnings or notices have been reported during import process.'), 'status');
        break;
    }

    if (!$options['line_checks']) {
      drupal_set_message(t('Line checks have been disabled. Some warnings and notices may have not been reported.'), 'warning');
    }

    if (!$options['internal_cache']) {
      drupal_set_message(t('No more information about process because internal cache is disabled.'));
    }
    else {
      // Display stats and eventually lists about imported terms.
      if ($options['result_stats'] || $options['result_terms']) {
        if (isset($options['imported_terms'])) {
          if ($options['result_terms']) {
            drupal_set_message(_taxonomy_csv_info_terms_stats($options['imported_terms'], 'full'), 'status');
          }
          else if ($options['result_stats']) {
            drupal_set_message(_taxonomy_csv_info_terms_stats($options['imported_terms'], 'stats'), 'status');
          }
        }
        else {
          drupal_set_message(t('No term was imported.'), 'warning');
        }
      }

      // Display detailled result of import.
      if ($options['result_level'] != 'none') {
        if (count($results)) {
          drupal_set_message(t('Available informations about lines import.'));
          call_user_func("_taxonomy_csv_info_result_{$options['result_type']}", $results, $options['result_level']);
        }
        else {
          drupal_set_message(t('No information reported about lines import.'));
        }
      }
    }
  }
}

/**
 * Create vocabulary by its name and return an array with its vid and its name.
 */
function taxonomy_csv_vocabulary_create($vocabulary_name = '') {
  if (is_string($vocabulary_name) || ($vocabulary_name == '')) {
    $new_vocabulary_name = _taxonomy_csv_vocabulary_create_name($vocabulary_name);

    // Create an empty vocabulary. Relations and hierarchy are updated after import.
    $vocabulary = array(
      'name'         => $new_vocabulary_name,
      'description'  => t('Auto created vocabulary by taxonomy_csv module'),
      'help'         => '',
      'relations'    => TRUE,
      'hierarchy'    => 2,
      'multiple'     => TRUE,
      'required'     => FALSE,
      'tags'         => FALSE,
      'module'       => 'taxonomy',
      'weight'       => 0,
      'nodes'        => array(),
    );

    taxonomy_save_vocabulary($vocabulary);

    return array(
      'vid' => taxonomy_csv_vocabulary_get_id($vocabulary['name']),
      'name' => $vocabulary['name'],
    );
  }
}

/**
 * Duplicate a vocabulary by its vid and return an array with vid and name.
 * If not exist, create an empty vocabulary.
 */
function taxonomy_csv_vocabulary_duplicate($vocabulary_id) {
  if ($original_vocabulary = taxonomy_vocabulary_load($vocabulary_id)) {
    // Creates an unused name. Check if name begins with 'Copy of #name' in order to serialize name.
    $name = t('Copy of [!vocabulary_name]', array('!vocabulary_name' => $original_vocabulary->name));
    $name = _taxonomy_csv_vocabulary_create_name((strpos($original_vocabulary->name, $name) === FALSE) ? $name : $original_vocabulary->name);

    // Duplicate original vocabulary, except relations and hierarchy, updated after import.
    $duplicated_vocabulary = array(
      'name'         => $name,
      'description'  => $original_vocabulary->description,
      'help'         => $original_vocabulary->help,
      'relations'    => TRUE,
      'hierarchy'    => 2,
      'multiple'     => $original_vocabulary->multiple,
      'required'     => $original_vocabulary->required,
      'tags'         => $original_vocabulary->tags,
      'module'       => $original_vocabulary->module,
      'weight'       => $original_vocabulary->weight,
      'nodes'        => array(),
    );

    taxonomy_save_vocabulary($duplicated_vocabulary);

    $duplicated_vocabulary['vid'] = taxonomy_csv_vocabulary_get_id($duplicated_vocabulary['name']);

    // Get all terms and attributes of original vocabulary
    // and copy them in the new one in two steps.
    $original_terms = taxonomy_get_tree($original_vocabulary->vid);

    // First step: copy each term except relations and parents.
    $duplicated_terms = array();
    foreach ($original_terms as $original_term) {
      $duplicated_terms[$original_term->tid] = array(
        'vid'         => $duplicated_vocabulary['vid'],
        'name'        => $original_term->name,
        'description' => $original_term->description,
        'weight'      => $original_term->weight,
        'synonyms'    => taxonomy_get_synonyms($original_term->tid),
      );
      taxonomy_csv_term_save($duplicated_terms[$original_term->tid]);

      // Complete term with its tid for next step.
      $duplicated_terms[$original_term->tid]['tid'] = db_last_insert_id('term_data', 'tid');
    }

    // Second step: update duplicated terms with parents and relations.
    foreach ($original_terms as $original_term) {
      $duplicated_terms[$original_term->tid]['parent'] = array();
      foreach ($original_term->parents as $original_parent_term_id) {
        $duplicated_terms[$original_term->tid]['parent'][] = $duplicated_terms[$original_parent_term_id]['tid'];
      }

      $duplicated_terms[$original_term->tid]['relations'] = array();
      $original_related_terms = taxonomy_get_related($original_term->tid);
      foreach ($original_related_terms as $original_related_term) {
        $duplicated_terms[$original_term->tid]['relations'][] = $duplicated_terms[$original_related_term->tid]['tid'];
      }

      taxonomy_csv_term_save($duplicated_terms[$original_term->tid]);
    }

    return array(
      'vid'  => $duplicated_vocabulary['vid'],
      'name' => $duplicated_vocabulary['name'],
    );
  }
  else {
    return taxonomy_csv_vocabulary_create();
  }
}

/**
 * Helper to create an unused vocabulary name from a string.
 */
function _taxonomy_csv_vocabulary_create_name($vocabulary_name = '') {
  $vocabulary_check_name = (drupal_strlen($name = eregi_replace('.csv', '', trim(basename($vocabulary_name)))) > 0) ? $name : t('Auto created vocabulary');

  // Limits to 250 characters.
  $vocabulary_check_name = drupal_substr($vocabulary_check_name, 0, 250);

  // Invent a unused vocabulary name.
  if (!taxonomy_csv_vocabulary_get_id($vocabulary_check_name)) {
    $new_vocabulary_name = $vocabulary_check_name;
  }
  else {
    for ($i = 2; taxonomy_csv_vocabulary_get_id("$vocabulary_check_name $i"); $i++) {
    }
    $new_vocabulary_name = "$vocabulary_check_name $i";
  }

  return $new_vocabulary_name;
}

/**
 * Static check and update hierarchy flag of a given vocabulary.
 * If hierarchy is set, simply update it without check.
 *
 * Drupal update hierarchy function works only dynamically.
 *
 * @param $vid
 *   The vocabulary id to update.
 * @param $hierarchy
 *   Optional. The hierarchy level to set (0, 1 or 2).
 * @return
 *   Updated hierarchy level or FALSE if not exist.
 */
function taxonomy_csv_vocabulary_check_hierarchy($vid, $hierarchy_level = FALSE) {
  $vocabulary = taxonomy_csv_vocabulary_get($vid);

  if ($vocabulary) {
    if ($hierarchy_level !== FALSE) {
      // Set hierarchy_level without check vocabulary.
      $hierarchy = $hierarchy_level;
    }
    else {
      $tree = taxonomy_get_tree($vid);
      $hierarchy = 0;
      foreach ($tree as $term) {
        // Check current term's parent count.
        if (count($term->parents) > 1) {
          $hierarchy = 2;
          break;
        }
        // Can't detect level 0 vocabulary if: 0 !== array_shift... Cf. taxonomy.module l. 285
        else if (count($term->parents) == 1 && 0 != array_shift($term->parents)) {
          $hierarchy = 1;
        }
      }
    }
    // Update hierarchy if necessary.
    if ($hierarchy != $vocabulary['hierarchy']) {
      $vocabulary['hierarchy'] = $hierarchy;
      $result = taxonomy_save_vocabulary($vocabulary);
    }

    return $hierarchy;
  }
  return FALSE;
}

/**
 * Return vocabulary array by its vid or FALSE if not exist.
 */
function taxonomy_csv_vocabulary_get($vid) {
  $result = db_query('SELECT v.* FROM {vocabulary} v WHERE v.vid = %d', $vid);
  return db_fetch_array($result);
}

/**
 * Return vocabulary vid by its name or NULL if none.
 */
function taxonomy_csv_vocabulary_get_id($vocabulary_name) {
  if (is_string($vocabulary_name) && ($vocabularies = taxonomy_get_vocabularies())) {
    foreach ($vocabularies as $vocabulary) {
      if ($vocabulary_name == $vocabulary->name) {
        return $vocabulary->vid;
      }
    }
  }
}

/**
 * @} End of "defgroup taxonomy_csv_vocabulary".
 */

/**
 * @defgroup taxonomy_csv_line Import a line
 * @{
 * Functions allowing import of a line of terms or fields.
 */

/**
 * Import a line that contains a term and other items matching the options.
 *
 * @param $line
 *   Array which contains items of a csv line.
 * @param $options
 *   Array of import options (see taxonomy_csv_vocabulary_import).
 * @param $previous_line
 *   Optional. Cleaned and checked previous imported line names and tids array.
 *   Needed with some contents as one term array structure import or taxonomy manager export.
 *   With taxonomy manager export, tid array is indexed with old tids.
 * @return Result array:
 *   'name' => array of imported terms names,
 *   'tid'  => array of imported terms tids,
 *   'msg'  => messages arrays:
 *     term position => array of status messages of term,
 *     'line'        => array of status messages of line.
 */
function taxonomy_csv_line_import($line, $options, $previous_line = array()) {
  // Define default values.
  $line_messages = array();
  $items_messages = array();
  $result = array(
    'name' => array(),
    'tid'  => array(),
    'msg'  => array(),
  );

  if ($options['line_checks']) {
    // 1. Validate and clean line.
    $line = _taxonomy_csv_line_clean(
      $line,
      $options['utf8_check'],
      $line_messages);
    if (_taxonomy_csv_messages_get_worst_level($line_messages, 'msg') > TAXONOMY_CSV_WATCHDOG_WARNING) {

      // 2. Check items of line.
      $line = _taxonomy_csv_line_check(
        $line,
        $options['source_content'],
        $options['import_existing_items'],
        $previous_line,
        $items_messages);
      $line_messages = array_merge($line_messages, $items_messages);
      if (_taxonomy_csv_messages_get_worst_level($items_messages, 'msg') > TAXONOMY_CSV_WATCHDOG_WARNING) {

      // 3. Process import items with full checked line.
        $result = _taxonomy_csv_line_process(
          $line,
          $options,
          $previous_line);
        // Add a line level message of success or error.
        $line_messages[] = (_taxonomy_csv_messages_get_worst_level($result, 'line') > TAXONOMY_CSV_WATCHDOG_WARNING) ? 699 : 499; // Successfully / badly imported line.
      }
    }
  }
  else {
    // No checks, so directly import line.
    $result = _taxonomy_csv_line_process(
      array_values(array_map('trim', $line)),
      $options,
      $previous_line);
    // Add a line level message of success or error.
    $line_messages[] = (_taxonomy_csv_messages_get_worst_level($result, 'line') > TAXONOMY_CSV_WATCHDOG_WARNING) ? 699 : 499; // Successfully / badly imported line.
  }

  // Keep previous items in case of an empty or an unprocessed line.
  if (count($result['name']) == 0) {
    $result['name'] = $previous_line['name'];
    $result['tid']  = $previous_line['tid'];
  }

  // Remember line level messages.
  $result['msg'] += array('line' => $line_messages);

  return $result;
}

/**
 * Helper function to clean an imported line.
 *
 * @param $line
 *   Array of items to be processed.
 * @param $utf8_check
 *   Optional. Enable (default) or not conversion to utf8.
 * @param &$messages
 *   By reference array of messages.
 * @return Array of cleaned imported line.
 */
function _taxonomy_csv_line_clean($line, $utf8_check = TRUE, &$messages) {
  $cleaned_line = array();

  // Example: string "Term 1".
  if (!is_array($line)) {
    $messages[] = 310; // Error not a line array.
  }
  // Example: " " or unrecognized line.
  else if ((count($line) == 0) || empty($line) || ((count($line) == 1) && (($line[0] == NULL) || (trim($line[0]) == '')))) {
    $messages[] = 493; // Warning empty line.
  }
  else {
    $cleaned_line = $line;

    ///TODO: To simplify.
    // Convert each item of the line to UTF-8. Support multiple encodings on same line.
    // No convert but check if already utf-8: corrects bug http://drupal.org/node/364832.
    if ($utf8_check) {
      if (function_exists('mb_detect_encoding')) {
        foreach ($line as $key => $item) {
          $enc = mb_detect_encoding($item, 'UTF-8, ISO-8859-1, ISO-8859-15', TRUE);
          if ($enc != 'UTF-8') {
            if (FALSE === $cleaned_line[$key] = drupal_utf8_check($item, $enc)) {
              $messages[] = 320; // Error convert.
            }
          }
        }
      }
      else {
        $messages[] = 320; // Error convert.
      }
    }
    else {
      // Security check if user say file is utf-8 encoded.
      foreach ($line as $item) {
        if (!drupal_validate_utf8($item)) {
          $messages[] = 321; // Error validate.
        }
      }
    }

    // Trim and check empty line: useful for some non-Ascii lines.
    $line = array_map('trim', $cleaned_line);

    /// TODO: To simplify.
    // Example: " , , ".
    $test_line = array_unique($line);
    if (count($test_line) == 1 && in_array('', $test_line)) {
      $messages[] = 491; // Warning no item.
    }
    else {
      $cleaned_line = array_values($line);
    }
  }

  return $cleaned_line;
}

/**
 * Helper function to validate an imported line.
 *
 * @param $line
 *   Array of items from a cleaned line.
 * @param $source_content
 *   Content of line.
 * @param $import_existing_items
 *   What become existing items with same names.
 * @param $previous_line
 *   Optional. Cleaned and checked previous imported line names and tids array.
 *   Needed with some contents as one term array structure import or taxonomy manager export.
 *   With taxonomy manager export, tid array is indexed with old tids.
 * @param &$messages
 *   By reference array of messages.
 * @return Array of checked items of imported line.
 */
function _taxonomy_csv_line_check($line, $source_content, $import_existing_items, $previous_line = array(), &$messages) {
  $checked_items = array();

  // No input check because line and previous line are already checked in previous function.
  // A php callback function may be used to simplify checking.
  switch ($source_content) {
    case TAXONOMY_CSV_SOURCE_ALONE_TERMS:
      // Checks empty first column.
      // Example: ", Item 1, Item 2"
      if (empty($line[0])) {
        $messages[] = 480; // Warning no first column
      }
      else {
        if (count($line) > 1) {
          $messages[] = 501; // Notice too many items.
        }
        $checked_items = array($line[0]);
      }
      break;

    case TAXONOMY_CSV_SOURCE_TERMS:
      $checked_items = array_unique(array_filter($line));
      if (count($checked_items) == 0) {
        $messages[] = 491; // Warning no item.
      }
      if (count($checked_items) < count($line)) {
        $messages[] = 531; // Notice duplicates.
      }
      break;

    case TAXONOMY_CSV_SOURCE_STRUCTURE:
      // Check last empty column before first item with matching previous imported column.
      for ($first_non_empty = 0; ($first_non_empty < count($line)) && (empty($line[$first_non_empty])); $first_non_empty++) {
      }

      // Example: Previous line("Term 1,Item 2") ; Current line(",,,Item4")
      if (($first_non_empty > 0) && (!isset($previous_line['name'][$first_non_empty - 1]))) {
        $messages[] = 410; // Warning impossible to get parent.
      }
      else {
        // Example: Previous line("Term 1,Item 2") ; Current line(",,,Item4")
        // "0" value are lost, but that is not important for a taxonomy.
        $imported_items = array_filter(array_slice($line, $first_non_empty));
        if (count($imported_items) == 0) {
          $messages[] = 491; // Warning no item.
        }
        else if (count($imported_items) < (count($line) - $first_non_empty)) {
          $messages[] = 510; // Notice empty items.
        }

        // Currently, keep previous import system for children, so no other checks (duplicates...).

        if ($first_non_empty == 0) {
          $checked_items = $imported_items;
        }
        else {
          $checked_items = array_merge(array_fill(0, $first_non_empty, ''), $imported_items);
        }
      }
      break;

    case TAXONOMY_CSV_SOURCE_PARENTS:
    case TAXONOMY_CSV_SOURCE_CHILDREN:
    case TAXONOMY_CSV_SOURCE_RELATIONS:
    case TAXONOMY_CSV_SOURCE_SYNONYMS:
      if (empty($line[0])) {
        $messages[] = 480; // Warning no first column
      }
      else {
        // Example: "Term,Item 1,,Item 2"
        // "0" value is lost, but this is not important for a taxonomy.
        $imported_items = array_filter(array_slice($line, 1));
        if (count($imported_items) < (count($line) - 1)) {
          $messages[] = 530; // Notice some empty items.
        }
        // Example: "Term,Item 1,Item 2,Item 1"
        $imported_unique_items = array_unique($imported_items);
        if (count($imported_unique_items) < (count($imported_items))) {
          $messages[] = 531; // Notice duplicates.
        }
        // Example: "Term,Item 1,Term,Item 2"
        $checked_items = array_unique(array_merge(array($line[0]), $imported_unique_items));
        if (count($checked_items) <= count($imported_unique_items)) {
          $messages[] = 532; // Notice first and some items are same.
        }
        // Example: "Term"
        if ((count($checked_items) == 1)
            && ($import_existing_items == TAXONOMY_CSV_EXISTING_UPDATE_REPLACE)) {
          $messages[] = 580; // Notice remove items.
        }
      }
      break;

    case TAXONOMY_CSV_SOURCE_DESCRIPTIONS:
      if (empty($line[0])) {
        $messages[] = 480; // Warning no first column
      }
      else {
        if ((count($line) == 1)
            && ($import_existing_items == TAXONOMY_CSV_EXISTING_UPDATE_REPLACE)) {
          $messages[] = 580; // Notice remove items.
          $checked_items = array($line[0], '');
        }
        else {
          if (count($line) > 2) {
            $messages[] = 541; // Notice too many items.
          }
          $checked_items = array($line[0], $line[1]);
        }
      }
      break;

    case TAXONOMY_CSV_SOURCE_WEIGHTS:
      if (empty($line[0])) {
        $messages[] = 480; // Warning no first column
      }
      else if ((count($line) > 1) && !is_numeric($line[1]) && $line[1]) {
        $messages[] = 450; // Warning weight is not a number.
      }
      else {
        if ((count($line) == 1) && ($import_existing_items == TAXONOMY_CSV_EXISTING_UPDATE_REPLACE)) {
          $messages[] = 580; // Notice remove items.
          $checked_items = array($line[0], 0);
        }
        else {
          if (count($line) > 2) {
            $messages[] = 541; // Notice too many items.
          }
          $checked_items = array($line[0], intval($line[1]));
        }
      }
      break;

    case TAXONOMY_CSV_SOURCE_MANAGER_EXPORT:
      $checked_items = $line;
      if (empty($line[0])) {
        $messages[] = 550; // Notice no first column.
      }
      if (count($line) < 4) {
        $messages[] = 450; // Warning some items lacks.
      }
      else if (empty($line[1]) || empty($line[2])
          || ($line[1] == 0) || ($line[2] == '')) {
        $messages[] = 481; // Warning no item in second or third column.
      }
      else {
        if (count($line) == 4) {
          $messages[] = 551; // Notice root term.
          $checked_items[] = 0;
        }
        foreach ($checked_items as $key => $value) {
          if (($key != 2) && ($key != 3) && !is_numeric($value)) {
            $messages[] = 451; // Warning not a number.
            break;
          }
        }
        $list_parents = array_unique(array_slice($checked_items, 4));
        sort($list_parents);
        if (count($list_parents) != count(array_slice($checked_items, 4))) {
          $messages[] = 531; // Notice duplicates.
        }
        foreach ($list_parents as $value) {
          if ($value == $checked_items[1]) {
            $messages[] = 452; // Warning a term can't be a parent of itself.
            break;
          }
        }
        $checked_items = array_merge(array_slice($checked_items, 0, 4), $list_parents);
        if (($list_parents[0] === 0) && (count($list_parents) > 1)) {
          $messages[] = 453; // Warning: root term has a parent.
        }
        // $previous_line keys contain all previous lines source tid.
        if (isset($previous_line['tid'][$checked_items[1]])) {
          $messages[] = 552; // Notice: term has been already imported.
        }
        foreach ($list_parents as $value) {
          if (($value != 0) && !isset($previous_line['tid'][$value])) {
            $messages[] = 352; // Error: parent tid hasn't been imported in a previous line.
            break;
          }
        }
      }
      break;
  }

  return array_values($checked_items);
}

/**
 * Helper function to process import items.
 *
 * @param $line
 *   Array which contains items of a cleaned and checked csv line.
 * @param $options
 *   Array of import options (see taxonomy_csv_vocabulary_import).
 * @param $previous_line
 *   Optional. Cleaned and checked previous imported line names and tids array.
 *   Needed with some contents as one term array structure import or taxonomy manager export.
 *   With taxonomy manager export, tid array is indexed with old tids.
 * @return Result array:
 *   'name' => array of imported terms names,
 *   'tid'  => array of imported terms tids,
 *   'msg'  => messages arrays:
 *     term position => array of status messages of term.
 */
function _taxonomy_csv_line_process($line, $options, $previous_line = array()) {
  // Define default values.
  $result = array(
    'name' => array(),
    'tid'  => array(),
    'msg'  => array(),
  );

  // Only count check because line and previous line already checked.
  if (count($line)) {
    switch ($options['source_content']) {
      case TAXONOMY_CSV_SOURCE_TERMS:
        foreach ($line as $term_name) {
          $term = array(
            'name' => $term_name,
            'vid'  => $options['destination_vocabulary_id'],
          );
          // Import term then store and check result.
          $current_result = taxonomy_csv_term_import($term, $options['import_existing_items'], FALSE, NULL);
          $result['name'][] = $current_result['name'];
          $result['tid'][]  = $current_result['tid'];
          $result['msg'][]  = $current_result['msg'];
          if (_taxonomy_csv_messages_get_worst_level($current_result, 'term') <= TAXONOMY_CSV_WATCHDOG_WARNING) {
            break;
          }
        }
        break;

      case TAXONOMY_CSV_SOURCE_STRUCTURE:
        // Calculate first non empty item as line can be a full, partial or one term array.
        for ($first_non_empty = 0; ($first_non_empty < count($line)) && (empty($line[$first_non_empty])); $first_non_empty++) {
        }

        // Clean line of surabondant previous items then memorize previous terms for next line.
        if ($first_non_empty > 0) {
          $result['name'] = $previous_line['name'] = array_slice($previous_line['name'], 0, $first_non_empty);
          $result['tid']  = $previous_line['tid']  = array_slice($previous_line['tid'], 0, $first_non_empty);
          foreach ($previous_line['tid'] as $key => $term) {
            $result['msg'][]  = array(693); // Previous line term;
          }
        }

        // Set root or previous ancestor id.
        $parent_tid = ($first_non_empty == 0) ? 0 : $previous_line['tid'][$first_non_empty - 1];
        for ($c = $first_non_empty; $c < count($line); $c++) {
          $term = array(
            'name'   => $line[$c],
            'vid'    => $options['destination_vocabulary_id'],
            // Need an array in case of multiple parent.
            // Parent[0] matches the current parent for line import.
            'parent' => array($parent_tid),
          );

          // Parent terms (so all terms but the last on this line) are always updated because they are successive parents of a child.
          $current_term_import_option = (($options['import_existing_items'] == TAXONOMY_CSV_EXISTING_IGNORE_CREATE) && ($c < count($line) - 1)) ? TAXONOMY_CSV_EXISTING_IGNORE_PREVIOUS : $options['import_existing_items'];

          // Import term then store and check result.
          $current_result = taxonomy_csv_term_import($term, $current_term_import_option, FALSE, $term['parent'][0]);
          $result['name'][] = $current_result['name'];
          $result['tid'][]  = $current_result['tid'];
          $result['msg'][]  = $current_result['msg'];

          if (_taxonomy_csv_messages_get_worst_level($current_result['msg'], 'msg') <= TAXONOMY_CSV_WATCHDOG_WARNING) {
            break;
          }

          $parent_tid = $current_result['tid'];
        }
        break;

      case TAXONOMY_CSV_SOURCE_PARENTS:
        // First, import parents.
        if (count($line) > 1) {
          $list_parents = array(
            'name' => array_slice($line, 1),
            'tid'  => array(),
          );
          foreach ($list_parents['name'] as $value) {
            $term = array(
              'name' => $value,
              'vid'  => $options['destination_vocabulary_id'],
            );

            // Import term then store and check result.
            $current_result = taxonomy_csv_term_import($term, $options['import_existing_items'], FALSE, NULL);
            $result['name'][] = $current_result['name'];
            $result['tid'][]  = $current_result['tid'];
            $result['msg'][]  = $current_result['msg'];

            if (_taxonomy_csv_messages_get_worst_level($current_result['msg'], 'msg') <= TAXONOMY_CSV_WATCHDOG_WARNING) {
              break 2;
            }

            $list_parents['tid'][] = $current_result['tid'];
          }
        }

        // Second, import main term.
        $term = array(
          'name'   => $line[0],
          'vid'    => $options['destination_vocabulary_id'],
          'parent' => $list_parents['tid'],
        );

        // Import term then store result. No check because last term.
        $current_result = taxonomy_csv_term_import($term, $options['import_existing_items'], FALSE, NULL);
        $result['name'][] = $current_result['name'];
        $result['tid'][]  = $current_result['tid'];
        $result['msg'][]  = $current_result['msg'];
        break;

      case TAXONOMY_CSV_SOURCE_CHILDREN:
        // First, import main term.
        $term = array(
          'name' => $line[0],
          'vid'  => $options['destination_vocabulary_id'],
        );

        // Import term then store and check result.
        $current_result = taxonomy_csv_term_import($term, $options['import_existing_items'], FALSE, NULL);
        $result['name'][] = $current_result['name'];
        $result['tid'][]  = $current_result['tid'];
        $result['msg'][]  = $current_result['msg'];

        if (_taxonomy_csv_messages_get_worst_level($current_result['msg'], 'msg') <= TAXONOMY_CSV_WATCHDOG_WARNING) {
          break;
        }

        // Remember tid of main term.
        $main_tid = $current_result['tid'];

        // Second, import children.
        if (count($line) > 1) {
          foreach (array_slice($line, 1) as $value) {
            $term = array(
              'name'   => $value,
              'vid'    => $options['destination_vocabulary_id'],
              'parent' => array($main_tid),
            );

            // Import term then store and check result.
            $current_result = taxonomy_csv_term_import($term, $options['import_existing_items'], FALSE, NULL);
            $result['name'][] = $current_result['name'];
            $result['tid'][]  = $current_result['tid'];
            $result['msg'][]  = $current_result['msg'];

            if (_taxonomy_csv_messages_get_worst_level($current_result['msg'], 'msg') <= TAXONOMY_CSV_WATCHDOG_WARNING) {
              break;
            }
          }
        }
        break;

      case TAXONOMY_CSV_SOURCE_RELATIONS:
        // Each related term should exist before first column term can be related to it (need of its tid). Next, process relations and eventually subrelations
        // So, with a line array (A,B,C,D,E...), creates A, BA, CA(B), DA(BC), EA(BCD)... as it's the fatest way to create one or all relations even if items not exists.
        // As use of name is only for first column, need to keep previous tids matching names in $previous_relations ([name] <=> [tid]).
        $previous_relations = array();
        foreach ($line as $key => $term) {
          $term = array(
            'name'      => $line[$key],
            'vid'       => $options['destination_vocabulary_id'],
            'relations' => array_slice($previous_relations, 0, ($options['relations_create_subrelations']) ? $key : $key != 0),
          );

          ///TODO: Set an 'ignore all' option.
          // For second and next terms, update merge related terms in order to keep previous relations, as option is only for the first column term.
          $current_term_import_option = ($key == 0) ? $options['import_existing_items'] : TAXONOMY_CSV_EXISTING_UPDATE_MERGE;

          $current_result = taxonomy_csv_term_import($term, $current_term_import_option, $options['relations_all_vocabularies'], NULL);
          $result['name'][] = $current_result['name'];
          $result['tid'][]  = $current_result['tid'];
          $result['msg'][]  = $current_result['msg'];

          if (_taxonomy_csv_messages_get_worst_level($current_result['msg'], 'msg') <= TAXONOMY_CSV_WATCHDOG_WARNING) {
            break;
          }

          $previous_relations[$current_result['name']] = $current_result['tid'];
        }
        break;

      case TAXONOMY_CSV_SOURCE_MANAGER_EXPORT:
        $term = array(
          'name'        => $line[2],
          'vid'         => $options['destination_vocabulary_id'],
        );
        if ($line[3]) {
          $term['description'] = $line[3];
        }

        // Complete with parents except root. All parents exist, because it's previously checked.
        $list_parents = array_slice($line, 4);
        if ($list_parents[0] != 0) {
          foreach ($list_parents as $value) {
            $term['parent'][] = $previous_line['tid'][$value];
          }
        }

        // Use tid if term is already imported.
        if (isset($previous_line['tid'][$line[1]])) {
          $term['tid'] = $previous_line['tid'][$line[1]];
        }
        else {
          // Allow to import duplicate terms.
          $options['import_existing_items'] = TAXONOMY_CSV_EXISTING_IGNORE_CREATE;
        }

        // Import term then store result. No check because only one term.
        $current_result = taxonomy_csv_term_import($term, $options['import_existing_items'], FALSE, NULL);
        // Keep old index.
        $result = $previous_line;
        $result['name'][$line[1]] = $current_result['name'];
        $result['tid'][$line[1]]  = $current_result['tid'];
        $result['msg'][$line[1]]  = $current_result['msg'];
        break;

      default:
        $term = array(
          'name' => $line[0],
          'vid'  => $options['destination_vocabulary_id'],
        );
        switch ($options['source_content']) {
          case TAXONOMY_CSV_SOURCE_ALONE_TERMS:
            break;

          case TAXONOMY_CSV_SOURCE_SYNONYMS:
            $term['synonyms'] = array_slice($line, 1);
            break;

          case TAXONOMY_CSV_SOURCE_DESCRIPTIONS:
            $term['description'] = $line[1];
            break;

          case TAXONOMY_CSV_SOURCE_WEIGHTS:
            $term['weight'] = $line[1];
            break;
        }
        // Import term then store result. No check because only one term.
        $current_result = taxonomy_csv_term_import($term, $options['import_existing_items'], FALSE, NULL);
        $result['name'][] = $current_result['name'];
        $result['tid'][]  = $current_result['tid'];
        $result['msg'][]  = $current_result['msg'];
    }
  }
  else {
    $result['msg'] = array(array(695)); // No term to process.
  }

  return $result;
}

/**
 * @} End of "defgroup taxonomy_csv_line".
 */

/**
 * @defgroup taxonomy_csv_term Import a term
 * @{
 * Functions allowing import of a term.
 */

/**
 * Update or create a term with the given name in the given vocabulary and
 * given parent.
 *
 * @param $term
 *   A term array to import. Term is an array containing:
 *     'name'        => term name string,
 *   and eventually, matching options:
 *     'tid'         => term id,
 *     'vid'         => the vocabulary id where to import,
 *     'parent'      => array of first level parent tids,
 *     'relations'   => array of related tids,
 *     'synonyms'    => array of synonyms terms names strings,
 *     'description' => description string,
 *     'weight'      => weight integer,
 *   Eventual parent[0] matches the current parent where to import.
 * @param $import_existing_items
 *   Optional. Type of import on existing terms. Default to ignore and create.
 * @param $all_vocabularies
 *   Optional. Boolean. Enlarge search in all vocabularies.
 *   Useful for related terms. Default to FALSE (only $term['vid'] vocabulary).
 * @param $parent_tid
 *   Optional. Boolean. Restrict search in parent ($term['parent'][0] and next).
 *   Useful for structure. Default to NULL (search in whole vocabulary).
 * @return array
 *    'name' => term name,
 *    'tid'  => term id,
 *    'msg'  => messages array.
 */
function taxonomy_csv_term_import($term, $import_existing_items = TAXONOMY_CSV_EXISTING_IGNORE_CREATE, $all_vocabularies = FALSE, $parent_tid = NULL) {
  $messages = array();

  if (isset($term)) {
    switch ($import_existing_items) {
      case TAXONOMY_CSV_EXISTING_UPDATE:
        // 'update' is equal to 'update_merge' or 'update_replace'.

      case TAXONOMY_CSV_EXISTING_UPDATE_MERGE:
        if ($existing_term = _taxonomy_csv_term_find_existing($term, $all_vocabularies, $parent_tid)) {
          if (isset($term['parent'])) {
            $existing_term['parent'] = array_unique(array_merge($existing_term['parent'], $term['parent']));
          }
          if (isset($term['relations'])) {
            $existing_term['relations'] = array_unique(array_merge($existing_term['relations'], $term['relations']));
          }
          if (isset($term['synonyms'])) {
            $existing_term['synonyms'] = array_unique(array_merge($existing_term['synonyms'], $term['synonyms']));
          }
          if (isset($term['description'])) {
            $existing_term['description'] = ((($i = trim($existing_term['description'])) <> '') ? $i ."\n" : '') . $term['description'];
          }
          // Weight is always updated as it is a simple number.
          if (isset($term['weight'])) {
            $existing_term['weight'] = $term['weight'];
          }
          $term = $existing_term;
        }
        break;

      case TAXONOMY_CSV_EXISTING_IGNORE_PREVIOUS:
        // Currently, keep previous import system for structure, so no update replace for parent structure.

      case TAXONOMY_CSV_EXISTING_UPDATE_REPLACE:
        if ($existing_term = _taxonomy_csv_term_find_existing($term, $all_vocabularies, $parent_tid)) {
          foreach (array(
              'parent',
              'relations',
              'synonyms',
              'description',
              'weight',
            ) as $key) {
            if (array_key_exists($key, $term)) {
              $existing_term[$key] = $term[$key];
            }
          }
          $term = $existing_term;
        }
        break;

      case TAXONOMY_CSV_EXISTING_IGNORE:
        // As 'ignore' is equal to 'ignore_create'.

      case TAXONOMY_CSV_EXISTING_IGNORE_CREATE:
        break;

      case TAXONOMY_CSV_EXISTING_IGNORE_ALL:
      ///TODO: IGNORE_ALL
    }

    $result = taxonomy_csv_term_save($term);

    $messages[] = ($result == SAVED_NEW) ? 690 : 691;

    // Update cache with new $term.
    if (_taxonomy_csv_term_get_previous_imported($term, $result) === FALSE) {
      $messages[] = 392; // Cache problem.
    }
  }
  else {
    $messages[] = 391; // Error: not a term.
  }
  return array(
    'msg'  => $messages,
    'name' => $term['name'],
    'tid'  => $term['tid'],
  );
}

/**
 * Find an existing term in cache by its tid else in vocabularies by its name.
 *
 * @param $term
 *   The term array to find.
 * @param $all_vocabularies
 *   Optional. Boolean. Search in all vocabularies.
 *   Useful for related terms. Default to FALSE (only $term['vid'] vocabulary).
 * @param $parent_tid
 *   Optional. The parent term id.
 *   Useful for structure. Default to NULL.
 * @return
 *   Found term array, empty array if not found or false in case of error.
 */
function _taxonomy_csv_term_find_existing($term, $all_vocabularies = FALSE, $parent_tid = NULL) {
  // Find existing term in cache. A term in cache gets always its tid. All terms with tid are cached.
  if ($term['tid']) {
    if (_taxonomy_csv_term_get_previous_imported('', 'cache_enabled') === TRUE) {
      // Return FALSE in case of error: a tid is given, but has not been cached.
      return _taxonomy_csv_term_get_previous_imported($term);
    }
    else {
      return taxonomy_csv_term_get($term['tid']);
    }
  }
  // If no tid, search in vocabularies. If name doesn't exist, return array().
  else if ($term['name']) {
    return taxonomy_csv_term_find($term['name'], ($all_vocabularies) ? 0 : $term['vid'], $parent_tid);
  }
  // ERROR: no tid neither name.
  return FALSE;
}

/**
 * Find or save an existing term in the imported terms cache, by its tid.
 *
 * @param $term
 *   The term array to get or save. Need only the term['tid'] to get it.
 *   If $process_type is 'set_cache', array of terms arrays with 'st' (status).
 *   If $process_type is 'use_cache', boolean.
 * @param $process_type
 *   Optional. Process type:
 *   'get'          : find a cached imported term (default),
 *   SAVED_NEW      : update the static cache with the given term as created,
 *   SAVED_UPDATED  : update the static cache with the given term as updated,
 *   'count'        : count cached terms,
 *   'set_cache'    : set cache,
 *   'dump'         : return all the cache,
 *   'clear'        : clear cache,
 *   'use_cache'    : use cache if $term is TRUE or disable it if $term is FALSE,
 *   'cache_enabled': inform if cache is used.
 * @return
 *   Found term array as describe in taxonomy_csv_term_import or false if error.
 */
function &_taxonomy_csv_term_get_previous_imported($term, $process_type = 'get') {
  // Options all_vocabularies and parent_tid aren't used as currently, import process is always the same.

  // Enable or disable cache.
  static $use_cache = TRUE;

  // Each full term array is cached by its tid with its status.
  static $imported_cache = array();

  if ($use_cache) {
    switch ($process_type) {
      // Currently, as to make a relation influes on two terms, cached term can be not up to date for relations.
      // So currently, update returned result when get. Useless update cache when get (and lost status).
      ///TODO: if modified relateds, update cached relateds too (and only them). Or use handles.
      case 'get':
        if (isset($imported_cache[$term['tid']])) {
          $result = $imported_cache[$term['tid']];
          $result['relations'] = taxonomy_csv_term_get_related_tids($result['tid']);
          unset($result['st']); // Status is used only internaly.
          return $result;
        }
        break;

      case SAVED_NEW:
        if ((isset($term['tid'])) && ($term['tid'] != 0)) {
          if (!isset($imported_cache[$term['tid']])) {
            $imported_cache[$term['tid']] = $term;
            $imported_cache[$term['tid']]['st'] = SAVED_NEW;
            return $term;
          }
          else  { // Already cached and user wants again to create it.
            $imported_cache[$term['tid']] = $term;
            $imported_cache[$term['tid']]['st'] = TAXONOMY_CSV_ERROR;
            return FALSE;
          }
        }
        break;

      case SAVED_UPDATED: // 'update updated'
        if ((isset($term['tid'])) && ($term['tid'] != 0)) {
          if (!isset($imported_cache[$term['tid']])) {
            $imported_cache[$term['tid']] = $term;
            $imported_cache[$term['tid']]['st'] = SAVED_UPDATED;
            return $term;
          }
          else {
            switch ($imported_cache[$term['tid']]['st']) {
              case SAVED_NEW:
              case TAXONOMY_CSV_NEW_UPDATED:
                // Unchange status but update term.
                $imported_cache[$term['tid']] = $term;
                $imported_cache[$term['tid']]['st'] = TAXONOMY_CSV_NEW_UPDATED;
                return $term;
                break;

              case SAVED_UPDATED:
                // Unchange status whether previous status was, but update term.
                $imported_cache[$term['tid']] = $term;
                $imported_cache[$term['tid']]['st'] = SAVED_UPDATED;
                return $term;
                break;

              default:
                $imported_cache[$term['tid']] = $term;
                $imported_cache[$term['tid']]['st'] = TAXONOMY_CSV_ERROR;
                return FALSE;
            }
          }
        }
        break;

      case 'count':
        return count($imported_cache);

      case 'set_cache':
        $use_cache = TRUE;
        $imported_cache = $term;
        return;

      case 'dump':
        return $imported_cache;

      case 'clear':
        $imported_cache = array();
        return;

      case 'use_cache':
        $use_cache = $term;
        return;

      case 'cache_enabled':
        return $use_cache;

      default:
        // $process_type error.
        return FALSE;
    }
  }
  else {
    // Cache disabled. Not a boolean to distinguish from
    return 'no_cache';
  }
}

/**
 * Get a term by its id.
 *
 * @param $tid
 *   The tid to get.
 * @return
 *   Found term array as describe in taxonomy_csv_term_import or an empty array.
 */
function taxonomy_csv_term_get($tid) {
  if ($term = db_fetch_array(db_query('SELECT * FROM {term_data} WHERE tid = %d', $tid))) {
    $term['parent']    = taxonomy_csv_term_get_parents_tids($term['tid']);
    $term['relations'] = taxonomy_csv_term_get_related_tids($term['tid']);
    $term['synonyms']  = taxonomy_get_synonyms($term['tid']);
  };
  return $term;
}

/**
 * Find, by its name, the first existing term in a given vocabulary and a given
 * parent.
 *
 * @param $name
 *   The name string to find.
 * @param $vid
 *   Optional. The vocabulary id where to search if any. Default to NULL (all).
 * @param $parent_tid
 *   Optional. The parent term id if any. Default to NULL (none).
 * @return
 *   Found term array as describe in taxonomy_csv_term_import or an empty array.
 */
function taxonomy_csv_term_find($name, $vid = 0, $parent_tid = 0) {
  $name = drupal_strtolower(trim($name));

  if (is_string($name) && (drupal_strlen($name) > 0)) {
    $sql = "
      SELECT t.tid, t.*, h.parent
      FROM {term_data} t
      INNER JOIN {term_hierarchy} h ON t.tid = h.tid
      WHERE '%s' LIKE LOWER(t.name)
    ";
    $args = array($name);

    if ($vid > 0) {
      $sql .= ' AND t.vid = %d';
      $args[] = $vid;
    }

    if ($parent_tid > 0) {
      $sql .= ' AND h.parent = %d';
      $args[] = $parent_tid;
    }

    $sql .= ' LIMIT 1';

    $result = db_query($sql, $args);
    $term   = db_fetch_array($result);

    if ($term) {
      $term['parent']    = taxonomy_csv_term_get_parents_tids($term['tid']);
      $term['relations'] = taxonomy_csv_term_get_related_tids($term['tid']);
      $term['synonyms']  = taxonomy_get_synonyms($term['tid']);
    }
  }
  else {
    $term = FALSE; // ERROR: no input name.
  }
  return $term;
}

/**
 * Save a term array by reference.
 *
 * Drupal taxonomy_save_term use a text area format to import synonyms.
 * This helper convert a synonym array into a string before using it.
 *
 * @param $term
 *   A term array to save by reference. Term is an array containing:
 *     'name'        => term name string,
 *     'vid'         => the vocabulary id,
 *   and eventually:
 *     'tid'         => term id,
 *     'parent'      => array of first level parent tids,
 *     'relations'   => array of related tids,
 *     'synonyms'    => array of synonyms terms names strings,
 *     'description' => description string,
 *     'weight'      => weight integer,
 *   Eventual parent[0] matches the current parent where to import.
 * @return
 *   Status value.
 */
function taxonomy_csv_term_save(&$term) {
  if (isset($term['synonyms'])) {
    $synonyms = $term['synonyms'];
    $term['synonyms'] = implode("\n", $term['synonyms']);
  }

  // Drupal taxonomy_save_term use a by reference variable so $term is automaticaly updated.
  // Return either SAVED_NEW or SAVED_UPDATED.
  $result = taxonomy_save_term($term);

  // Keep synonyms as an array.
  if (isset($synonyms)) {
    $term['synonyms'] = $synonyms;
  }

  return $result;
}

/**
 * Return an array of all parents term IDs of a given term ID.
 */
function taxonomy_csv_term_get_parents_tids($tid) {
  $parents_tids = array();
  if (!empty($tid)) {
    if ($parents_terms = taxonomy_get_parents($tid)) {
      foreach ($parents_terms as $term => $item) {
        $parents_tids[] = $parents_terms[$term]->tid;
      }
    }
  }
  return $parents_tids;
}

/**
 * Return an array of all term IDs related to a given term ID.
 */
function taxonomy_csv_term_get_related_tids($tid) {
  $related_tids = array();
  if (!empty($tid)) {
    if ($related_terms = taxonomy_get_related($tid)) {
      foreach ($related_terms as $term => $item) {
        $related_tids[] = $related_terms[$term]->tid;
      }
    }
  }
  return $related_tids;
}

/**
 * @} End of "defgroup taxonomy_csv_term".
 */

/**
 * @defgroup taxonomy_csv_errors Errors helpers
 * @{
 * Functions managing import errors.
 */

/**
 * Helper to determine worst level of a result messages array.
 *
 * @param $messages
 *   Array of messages of an import line results.
 * @return worst message.
 */
function _taxonomy_csv_messages_get_worst_message($messages) {
  if (is_array($messages)) {
    ///TODO: to simplify.
    $list_messages = array();
    foreach ($messages as $value) {
      foreach ($value as $msg) {
        $list_messages[$msg] = $msg;
      }
    }
    // Sort so first is the lowest.
    sort($list_messages);

    return (count($list_messages)) ? $list_messages[0] : 998;   // No message.
  }
  return 0;
}

/**
 * Helper to determine worst message level of a set of message codes.
 *
 * @param $messages
 *   Array of message code (000 to 999) or set of full results messages.
 * @param $type
 *   Type of messages array:
 *     'msg' (default), 'term', 'line', 'lines', 'results'.
 * @return
 *   Worst level (0 to 9).
 */
function _taxonomy_csv_messages_get_worst_level($messages, $type = 'msg') {
  if (is_array($messages)) {
    switch ($type) {
      case 'msg':
        $list_messages = $messages;
        break;

      case 'term':
        $list_messages  = $messages['msg'];
        break;

      case 'line':
        $messages = array_shift($messages);
        $list_messages  = $messages['msg'];
        break;

      case 'lines':
        $list_messages = array();
        foreach ($messages as $line) {
          $list_messages = array_merge($list_messages, $line);
        }
        break;

      case 'results':
        // Convert [line number]['line'/serial]['msg'][message codes]
        //      to [message codes] => $message_code.
        if (count($messages) > 0) {
          $list_messages = array();
          foreach ($messages as $line_number => $operation) {
            foreach ($operation as $item) {
              foreach ($item['msg'] as $message_code) {
                $list_messages[$message_code] = $message_code;
              }
            }
          }
        }
        else {
          return 0;
        }
        break;

      default:
        return 0;
    }

    return count($list_messages) ? _taxonomy_csv_messages_get_level(min($list_messages)) : TAXONOMY_CSV_WATCHDOG_NONE;
  }
  return 0;
}

/**
 * Helper to determine error level of a message code.
 *
 * @param $message_code
 *   Single message code (000 to 999).
 * @return
 *   Level (0 to 9).
 */
function _taxonomy_csv_messages_get_level($message_code) {
  $result = intval($message_code / 100);
  return ($result >= TAXONOMY_CSV_WATCHDOG_ERROR && $result <= TAXONOMY_CSV_WATCHDOG_NONE) ? $result : TAXONOMY_CSV_WATCHDOG_ERROR;
}

/**
 * @} End of "defgroup taxonomy_csv_errors".
 */

/**
 * @defgroup taxonomy_csv_infos Informations helpers.
 * @{
 * Functions informing about import process.
 */

/**
 * Return formated list of options in order to inform user.
 */
function _taxonomy_csv_info_chosen_options($options) {
  // File or text area.
  $message = (($options['source_choice'] == 'filepath') ? t('File "%file" uploaded.', array('%file' => $options['file']->filename)) : t('Import terms from text.')) .'<br />';

  // Content.
  $list = _taxonomy_csv_lists('list_source_content_types');
  $message .= t('Source content: "%source_content".', array(
    '%source_content' => $list[$options['source_content']],
  )) .'<br />';

  // Existing items.
  $list = _taxonomy_csv_lists('list_import_options');
  $message .= t('Existing terms choice: "%import_items".', array(
      '%import_items' => $list[$options['import_existing_items']],
  )) .'<br />';

  return $message;
}

/**
 * Helper to remember some items.
 *
 * @param $list
 *   A string matching list to be returned:
 *     'list_source_content_types',
 *     'list_import_options',
 *     'list_source_content_allowed_import_options',
 *     'hierarchy_text',
 * @return wanted content.
 */
function _taxonomy_csv_lists($list) {
  switch ($list) {
    case 'list_source_content_types':
      // Some import types will be enabled in a next release.
      return array(
        TAXONOMY_CSV_SOURCE_ALONE_TERMS    => t('Terms in first column and ignore additional columns'),
        TAXONOMY_CSV_SOURCE_TERMS          => t('Terms (flat vocabulary)'),
        TAXONOMY_CSV_SOURCE_STRUCTURE      => t('Hierarchical structure or one term by line structure'),
        TAXONOMY_CSV_SOURCE_PARENTS        => t('First level parents of a term'),
        TAXONOMY_CSV_SOURCE_CHILDREN       => t('First level children of a term'),
        TAXONOMY_CSV_SOURCE_RELATIONS      => t('Related terms'),
        TAXONOMY_CSV_SOURCE_DESCRIPTIONS   => t('Term descriptions'),
        TAXONOMY_CSV_SOURCE_WEIGHTS        => t('Term weights'),
        TAXONOMY_CSV_SOURCE_SYNONYMS       => t('Synonyms terms'),
        TAXONOMY_CSV_SOURCE_MANAGER_EXPORT => t('Taxonomy manager export'),
      );

    case 'list_import_options':
      return array(
        TAXONOMY_CSV_EXISTING_UPDATE           => t('Update existing term'),
        TAXONOMY_CSV_EXISTING_UPDATE_MERGE     => t('Update (merge) existing term and items if exist else create'),
        TAXONOMY_CSV_EXISTING_UPDATE_REPLACE   => t('Update (replace) existing term and items if exist else create'),
        TAXONOMY_CSV_EXISTING_IGNORE           => t('Duplicate existing term'),
        TAXONOMY_CSV_EXISTING_IGNORE_CREATE    => t('Ignore existing term and create a new term'),
        TAXONOMY_CSV_EXISTING_IGNORE_ALL       => t('Ignore existing term and create a new term for each term on the line'),
      );

    case 'list_source_content_allowed_import_options':
      return array(
        TAXONOMY_CSV_SOURCE_ALONE_TERMS    => array(
          TAXONOMY_CSV_EXISTING_UPDATE,
          TAXONOMY_CSV_EXISTING_IGNORE,
        ),
        TAXONOMY_CSV_SOURCE_TERMS          => array(
          TAXONOMY_CSV_EXISTING_UPDATE,
          TAXONOMY_CSV_EXISTING_IGNORE,
        ),
        TAXONOMY_CSV_SOURCE_STRUCTURE      => array(
//          TAXONOMY_CSV_EXISTING_UPDATE_MERGE,
          TAXONOMY_CSV_EXISTING_UPDATE_REPLACE,
          TAXONOMY_CSV_EXISTING_IGNORE_CREATE,
//           TAXONOMY_CSV_EXISTING_IGNORE_ALL,
        ),
        TAXONOMY_CSV_SOURCE_PARENTS        => array(
          TAXONOMY_CSV_EXISTING_UPDATE_MERGE,
          TAXONOMY_CSV_EXISTING_UPDATE_REPLACE,
          TAXONOMY_CSV_EXISTING_IGNORE_CREATE,
//           TAXONOMY_CSV_EXISTING_IGNORE_ALL,
        ),
        TAXONOMY_CSV_SOURCE_CHILDREN       => array(
          TAXONOMY_CSV_EXISTING_UPDATE_MERGE,
          TAXONOMY_CSV_EXISTING_UPDATE_REPLACE,
          TAXONOMY_CSV_EXISTING_IGNORE_CREATE,
//           TAXONOMY_CSV_EXISTING_IGNORE_ALL,
        ),
        TAXONOMY_CSV_SOURCE_RELATIONS      => array(
          TAXONOMY_CSV_EXISTING_UPDATE_MERGE,
          TAXONOMY_CSV_EXISTING_UPDATE_REPLACE,
          TAXONOMY_CSV_EXISTING_IGNORE_CREATE,
//           TAXONOMY_CSV_EXISTING_IGNORE_ALL,
        ),
        TAXONOMY_CSV_SOURCE_DESCRIPTIONS   => array(
          TAXONOMY_CSV_EXISTING_UPDATE_MERGE,
          TAXONOMY_CSV_EXISTING_UPDATE_REPLACE,
          TAXONOMY_CSV_EXISTING_IGNORE_CREATE,
        ),
        TAXONOMY_CSV_SOURCE_WEIGHTS        => array(
          TAXONOMY_CSV_EXISTING_UPDATE_MERGE,
          TAXONOMY_CSV_EXISTING_IGNORE_CREATE,
        ),
        TAXONOMY_CSV_SOURCE_SYNONYMS       => array(
          TAXONOMY_CSV_EXISTING_UPDATE_MERGE,
          TAXONOMY_CSV_EXISTING_UPDATE_REPLACE,
          TAXONOMY_CSV_EXISTING_IGNORE_CREATE,
        ),
        TAXONOMY_CSV_SOURCE_MANAGER_EXPORT => array(
          TAXONOMY_CSV_EXISTING_UPDATE_MERGE,
          TAXONOMY_CSV_EXISTING_UPDATE_REPLACE,
          TAXONOMY_CSV_EXISTING_IGNORE_CREATE,
        ),
      );

    case 'hierarchy_text':
      return array(
        0 => t('no parent (flat)'),
        1 => t('single parent (tree)'),
        2 => t('multiple parents (polyhierarchy)'),
      );
  }
}

/**
 * Return informations about destination vocabulary.
 */
function _taxonomy_csv_info_vocabulary_destination($vocabulary, $destination_target) {
  // Destination vocabulary.
  $list_message = array(
    'autocreate' => t('New vocabulary "%vocabulary_name" has been created.', array('%vocabulary_name' => $vocabulary->name)),
    'duplicate'  => t('Duplicate vocabulary "%vocabulary_name" has been created.', array('%vocabulary_name' => $vocabulary->name)),
    'existing'   => t('Terms are imported into existing vocabulary "%vocabulary_name".', array('%vocabulary_name' => $vocabulary->name)),
  );

  return $list_message[$destination_target] .'<br />';
}

/**
 * Return informations about result vocabulary.
 */
function _taxonomy_csv_info_vocabulary_result($vocabulary, $destination_target, $new_hierarchy) {
  $message = '';

  // Level of vocabulary.
  $hierarchy_text = _taxonomy_csv_lists('hierarchy_text');
  if ($destination_target != 'autocreate') {
    $message .= t('Old vocabulary hierarchy level was %level (%level_text).', array(
      '%level'      => $vocabulary->hierarchy,
      '%level_text' => $hierarchy_text[$vocabulary->hierarchy],
    )) .'<br />';
  }
  $message .= t('Imported vocabulary hierarchy level is %level (%level_text).', array(
    '%level'      => $new_hierarchy,
    '%level_text' => $hierarchy_text[$new_hierarchy],
  )) .'<br />';

  // Direct links to edit vocabulary and view terms.
  $message .= t('Properties can be edited at <a href="!vocabulary_edit_link">Administer > Content management > Taxonomy > edit vocabulary</a>.', array(
    '!vocabulary_edit_link' => url("admin/content/taxonomy/edit/vocabulary/$vocabulary->vid"),
  )) .'<br />';
  $message .= t('You can view terms at <a href="!vocabulary_list">Administer > Content management > Taxonomy > list terms</a>.', array(
    '!vocabulary_list' => url("admin/content/taxonomy/$vocabulary->vid"),
  )) .'<br />';
  if (module_exists('taxonomy_manager')) {
    $message .= t('It is recommended to use <a href="!link" title="Taxonomy manager module">Administer > Taxonomy manager > Vocabularies</a> with big taxonomies because a memory error can occur with default Drupal "List terms" menu.', array('!link' => url("admin/content/taxonomy_manager/voc/$vocabulary->vid")));
  }
  else {
    $message .= t('Warning: with default Drupal "List terms" function, a memory error can occurs with big taxonomies, so use another manager as <a href="!link" title="Taxonomy manager module">Taxonomy manager</a>.', array('!link' => url('http://drupal.org/project/taxonomy_manager')));
  }
  return $message;
}

/**
 * Helper to display basic statistics about created and updated terms.
 *
 * @param $imported_terms
 *   Array of imported terms arrays with 'tid', 'name' and 'st' (status).
 * @param $result_level
 *   Optional. Level of messages to display. Default to 'full'. Anything else is not full.
 * @return
 *   Statistics on imported terms (created / created and updated / updated / error)
 *   with or without list of terms.
 */
function _taxonomy_csv_info_terms_stats(&$imported_terms, $result_level = 'full') {
  if (is_array($imported_terms)) {
    $stats_imported_terms = _taxonomy_csv_info_terms_stats_lists($imported_terms);

    $list_imported_text = array(
      SAVED_UPDATED            => t('Total updated existing terms'),
      SAVED_NEW                => t('Total created terms'),
      TAXONOMY_CSV_NEW_UPDATED => t('Total created and updated terms'),
      TAXONOMY_CSV_ERROR       => t('Total processed import errors'),
    );

    $processed_message_text = '';
    foreach ($list_imported_text as $imported => $imported_text) {
      $processed_message_text .= "$imported_text : ". count($stats_imported_terms[$imported]) .'<br />';
      if ((count($stats_imported_terms[$imported]) > 0)
          && ($result_level == 'full')) {
        $processed_message_text .= '"'. implode('", "', $stats_imported_terms[$imported]) .'".<br />';
      }
    }
    return $processed_message_text;
  }
}

/**
 * Helper to display basic statistics about created and updated terms.
 *
 * @param $imported_terms
 *   Array of imported terms arrays with 'tid', 'name' and 'st' (status).
 * @return
 *   Array of created / created and updated / updated / error arrays of terms name.
 */
function _taxonomy_csv_info_terms_stats_lists(&$imported_terms) {
  if (is_array($imported_terms)) {
    $stats_imported_terms = array(
      SAVED_NEW                => array(),
      SAVED_UPDATED            => array(),
      TAXONOMY_CSV_NEW_UPDATED => array(),
      TAXONOMY_CSV_ERROR       => array(),
    );

    foreach ($imported_terms as $key => $term) {
      if (!isset($term['st'])) {
        $term['st'] = TAXONOMY_CSV_ERROR;
      }
      $stats_imported_terms[$term['st']][] = $term['name'];
    }
  return $stats_imported_terms;
  }
}

/**
 * Helper to display by line list of result messages.
 *
 * @param $messages_set
 *   Array of operation array of message codes.
 *   Message code is an integer between 000 and 999.
 * @param $result_level
 *   Optional. Level of messages to display: 'full' (default), 'infos', 'notices', 'warnings'.
 * @return
 *   Worst level (0 to 9).
 */
function _taxonomy_csv_info_result_by_line($messages_set, $result_level = 'full') {
  $list_messages = array();

  if (is_array($messages_set) && (count($messages_set) > 0)) {
    // Prepare list of messages.
    switch ($result_level) {
      case 'full':
        // Convert [line number]['line'/serial]['msg'][message codes]
        //      to [line number][message codes].
        foreach ($messages_set as $line_number => $operation) {
          foreach ($operation as $item) {
            foreach ($item['msg'] as $message_code) {
              $list_messages[$line_number][] = $message_code;
            }
          }
        }
        break;

      case 'infos':
      case 'notices':
      case 'warnings':
        // Convert [line number][message codes]
        //      to [message codes][line number].
        foreach ($messages_set as $line_number => $operation) {
          foreach ($operation as $message_code) {
            $list_messages[$line_number][] = $message_code;
          }
        }
        break;

      default:
        return;
    }

    foreach ($list_messages as $line_number => $message_codes) {
      $processed_message_level = _taxonomy_csv_messages_get_worst_level($message_codes, 'msg');
      $processed_message_text = t('Line #!line_number:', array('!line_number' => $line_number)) .'<br />';

      foreach ($message_codes as $message_code) {
        $processed_message_text .= _taxonomy_csv_info_result_text($message_code) .'<br />';
      }

      switch ($processed_message_level) {
        case TAXONOMY_CSV_WATCHDOG_ERROR:
        case TAXONOMY_CSV_WATCHDOG_WARNING:
          $status = 'error';
          break;
        case TAXONOMY_CSV_WATCHDOG_NOTICE:
          $status = 'warning';
          break;
        case TAXONOMY_CSV_WATCHDOG_INFO:
          $status = 'status';
          break;
      }

      drupal_set_message($processed_message_text, $status);
    }

    return _taxonomy_csv_messages_get_worst_level($list_messages, 'lines');
  }
}

/**
 * Helper to display compact list of result messages.
 *
 * @param $messages_set
 *   Array of operation array of message codes, except 690 and 691 (imported terms).
 *   Message code is an integer between 000 and 999.
 * @param $result_level
 *   Optional. Level of messages to display: 'full' (default), 'infos', 'notices', 'warnings'.
 * @return
 *   Worst level (0 to 9).
 */
function _taxonomy_csv_info_result_by_message($messages_set, $result_level = 'full') {
  $list_messages = array();

  if (is_array($messages_set) && (count($messages_set) > 0)) {
    // Prepare list of messages.
    switch ($result_level) {
      case 'full':
        // Convert [line number]['line'/serial]['msg'][message codes]
        //      to [message codes][line number].
        foreach ($messages_set as $line_number => $operation) {
          foreach ($operation as $item) {
            foreach ($item['msg'] as $message_code) {
              $list_messages[$message_code][] = $line_number;
            }
          }
        }
        break;

      case 'infos':
      case 'notices':
      case 'warnings':
        // Convert [line number][message codes]
        //      to [message codes][line number].
        foreach ($messages_set as $line_number => $operation) {
          foreach ($operation as $message_code) {
            $list_messages[$message_code][] = $line_number;
          }
        }
        break;

      default:
        return;
    }
    ksort($list_messages);

    foreach ($list_messages as $message_code => $line_numbers) {
      // Exception for created and updated terms: show terms and not lines.
      if (($message_code != 690) && ($message_code != 691)) {
        $processed_message_level = _taxonomy_csv_messages_get_level($message_code);
        $processed_message_text = _taxonomy_csv_info_result_text($message_code) .' '. t('Lines:') .'<br />'. implode(", ", $line_numbers) .'.';

      switch ($processed_message_level) {
        case TAXONOMY_CSV_WATCHDOG_ERROR:
        case TAXONOMY_CSV_WATCHDOG_WARNING:
          $status = 'error';
          break;
        case TAXONOMY_CSV_WATCHDOG_NOTICE:
          $status = 'warning';
          break;
        case TAXONOMY_CSV_WATCHDOG_INFO:
          $status = 'status';
          break;
      }

      drupal_set_message($processed_message_text, $status);
      }
    }

    return _taxonomy_csv_messages_get_worst_level(array_keys($list_messages), 'msg');
  }
}

/**
 * Helper to get text of a message with a message code.
 *
 * @param $message_code
 *   Message codes are integer between 000 and 999.
 * @return
 *   Message text string.
 */
function _taxonomy_csv_info_result_text($message_code) {
  $watchdog_messages = array(
    // Format: Level, Type of import, Serial.
      0 => t('Module error'),

    300 => t('ERROR'),
    305 => t('Error in options.'),
    310 => t("You choose to import a taxonomy by a file, but you don't set its name or its size is greater than the server's limit. Import stopped."),
    311 => t('Size of your file is null.'),
    320 => t("Your file can't be converted to utf-8. Please install iconv, GNU recode or mbstring for PHP or convert your file to utf-8 and disable 'Enable utf-8 conversion' option. Import stopped."),
    321 => t('Despite your option, your file is not recognize as an UTF-8 encoded one. Convert it before submit it. Import stopped.'),
    352 => t("A parent tid is given, but it hasn't been imported in a previous line. Import stopped."),
    390 => t('No line to import. Please first check your file and file uploading, else reinstall module from a fresh release or submit an issue.'),
    391 => t('No term to import. Please first check your file and file uploading, else reinstall module from a fresh release or submit an issue.'),
    392 => t('Problem when caching imported term. Please first check your file and file uploading, else reinstall module from a fresh release or submit an issue.'),
    399 => t("Module is not installed correctly. Please reinstall module from a fresh release or submit an issue."),

    400 => t('WARNING'),
    410 => t("Impossible to get parent of first item, because previous line has less parent(s). You may add one or more parents to current line or change lines order. This line won't be processed."),
    450 => t("Weight in second column is not an allowed number. This line won't be processed."),
    451 => t("One or more items are not numbers. This line won't be processed."),
    452 => t("A term can't be a parent of itself. This line won't be processed."),
    453 => t("A root term has a parent. This line won't be processed."),
    480 => t("No first column term to import. Empty first column is allowed only with structure or multiple terms import. This line won't be processed."),
    481 => t("No item in second or third column. This line won't be processed."),
    482 => t("Some items aren't present. This line won't be processed."),
    490 => t("No line to import. This line won't be processed."),
    491 => t("No item to import. This line won't be processed."),
    492 => t("Nothing to import. This line won't be processed."),
    493 => t("Empty line. This line won't be processed."),
    499 => t('Warnings have been reported on this line.'),

    500 => t('Notice'),
    501 => t('Too many items. Second and next columns will be ignored, as import choice is to ignore them.'),
    510 => t('Line contains empty items after first item to import. They will be ignored.'),
    511 => t('Line contains duplicate items. Currently, duplicates will be ignored.'),
    512 => t("Line contains items matching first column term. A term can't be related to itself and a synonym may be different to it. Duplicates will be ignored."),
    530 => t('Line contains empty items. They will be ignored.'),
    531 => t('Line contains duplicate items. Duplicates will be ignored.'),
    532 => t("Line contains items matching first column term. A term can't be related to itself and a synonym may be different to it. Duplicates will be ignored."),
    541 => t('Too many items. Third and next columns will be ignored, as a term gets only one description and one weight.'),
    550 => t("No first column, but line can be processed."),
    551 => t('No parent, so it is a root term.'),
    552 => t('Term has been already imported in a previous line.'),
    580 => t('Be careful. This line has only one term and import choice is to replace existing items. So they will be removed.'),

    600 => t('Info'),
    605 => t('No error in options.'),
    690 => t('Saved new term.'),
    691 => t('Updated term.'),
    692 => t('Removed existing term.'),
    693 => t('Previous line term.'),
    695 => t('No term to process.'),
    696 => t('Empty line.'),
    697 => t('Command line.'),
    698 => t('Comment line.'),
    699 => t('Items of the line have been successfully imported.'),

    700 => t('Debug'),
    798 => t('Unreferenced debug code. Please reinstall module from a fresh release or submit an issue.'),
    799 => t('Unreferenced code.'),

    998 => t('No message.'),
    999 => t('No error.'),
  );

  if (is_int($message_code) && ($message_code >= 0) && ($message_code <= 999)) {
    $message_title = intval($message_code / 100) * 100;
    $message_title = $watchdog_messages[(isset($watchdog_messages[$message_title])) ? $message_title : 700] .' : ';

    return $message_title . $watchdog_messages[(isset($watchdog_messages[$message_code])) ? $message_code : 798];
  }
  return "$watchdog_messages[799] : $message_code";
}

/**
 * @} End of "defgroup taxonomy_csv_infos".
 */
