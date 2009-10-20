<?php
// $Id$

/**
 * @file
 * This light and simplified version of the full Api taxonomy_csv.api.inc allows
 * to use taxonomy_csv import functions from any other module.
 *
 * Only line level functions are available. No check are done on submitted
 * terms, neither before nor after importation.
 *
 * Example use:
 * @code
 * $result = taxonomy_csv_line_import(
 *   array(Europe, France, Paris),
 *   array(
 *    'import_format'  => 'tree_structure',
 *    'vocabulary_id'  => 2,
 *    'existing_items' => 'update_replace',
 * ));
 * @endcode
 */

/**
 * Available import schemas.
 */
define('TAXONOMY_CSV_FORMAT_ALONE_TERMS',       'alone_terms');
define('TAXONOMY_CSV_FORMAT_FLAT',              'flat');
define('TAXONOMY_CSV_FORMAT_TREE_STRUCTURE',    'tree_structure');
define('TAXONOMY_CSV_FORMAT_POLYHIERARCHY',     'polyhierarchy');
define('TAXONOMY_CSV_FORMAT_PARENTS',           'parents');
define('TAXONOMY_CSV_FORMAT_CHILDREN',          'children');
define('TAXONOMY_CSV_FORMAT_FIELDS',            'fields');
define('TAXONOMY_CSV_FORMAT_RELATIONS',         'relations');
define('TAXONOMY_CSV_FORMAT_DESCRIPTIONS',      'descriptions');
define('TAXONOMY_CSV_FORMAT_WEIGHTS',           'weights');
define('TAXONOMY_CSV_FORMAT_SYNONYMS',          'synonyms');
define('TAXONOMY_CSV_FORMAT_TAXONOMY_MANAGER',  'taxonomy_manager');

/**
 * Available import options.
 */
define('TAXONOMY_CSV_EXISTING_UPDATE',          'update'); // When no difference between merge and replace.
define('TAXONOMY_CSV_EXISTING_UPDATE_MERGE',    'update_merge');
define('TAXONOMY_CSV_EXISTING_UPDATE_REPLACE',  'update_replace');
define('TAXONOMY_CSV_EXISTING_IGNORE',          'ignore'); // When no difference between create and all. Equal to duplicate.
define('TAXONOMY_CSV_EXISTING_IGNORE_CREATE',   'ignore_create');
define('TAXONOMY_CSV_EXISTING_IGNORE_ALL',      'ignore_all'); // Ignore even existing terms in additional columns.
define('TAXONOMY_CSV_EXISTING_IGNORE_PREVIOUS', 'ignore_previous'); // Internal only.

/**
 * @defgroup taxonomy_csv_line Import a line
 * @{
 * Functions allowing import of a line of terms or fields.
 */

/**
 * Import a line that contains a term and other items matching the options.
 *
 * @param $line
 *   Array which contains items to import.
 * @param $options
 *   Array of import options:
 *     import_format  : see _taxonomy_csv_info_lists('list_import_format')
 *     vocabulary_id  : vocabulary id to import into
 *     existing_items : see _taxonomy_csv_info_lists('list_import_option')
 *     relations_create_subrelations: boolean. Specific to related terms import
 *     relations_all_vocabularies   : boolean. Id
 *   Only destination vocabulary id is needed: other variables get default values.
 * @param $previous_items
 *   Optional. Cleaned and checked previous imported line names and tids array.
 *   Needed with some contents as one term array structure import or taxonomy manager export.
 *   With taxonomy manager export, tid array is indexed with old tids.
 * @return Result array:
 *     'name' => array of imported terms names,
 *     'tid'  => array of imported terms tids,
 *   or FALSE if error.
 *   Warning: this returned value is different of taxonomy.csv.api.inc (always
 *   return array of messages codes and never FALSE).
 */
function taxonomy_csv_line_import($line, $options, $previous_items = array()) {
  // Set default value for unset options.
  foreach (array(
      'import_format'  => TAXONOMY_CSV_FORMAT_ALONE_TERMS,
      'existing_items' => TAXONOMY_CSV_EXISTING_UPDATE,
      // Default options of specific imports.
      'relations_create_subrelations' => FALSE,
      'relations_all_vocabularies'    => FALSE,
    ) as $key => $value) {
    if (!isset($options[$key])) {
      $options[$key] = $value;
    }
  }

  // Check if 'import_format' and 'existing_items' are compatible.
  $allowed = _taxonomy_csv_info_lists('list_import_format_allowed_import_option');
  if (!array_key_exists($options['import_format'], _taxonomy_csv_info_lists('list_import_format'))
      || !array_key_exists($options['existing_items'], _taxonomy_csv_info_lists('list_import_option'))
      || !in_array($options['existing_items'], $allowed[$options['import_format']])) {
    return FALSE;
  }

  // Call process import.
  $result = _taxonomy_csv_line_import_process(
    array_values(array_map('trim', $line)),
    $options,
    $previous_items);

  // Check ['msg'] array. TRUE in a msg means that there is a issue.
  foreach ($result['msg'] as $key => $value) {
    if (in_array(TRUE, $value, TRUE)) {
      return FALSE;
    }
  }
  unset($result['msg']);

  return $result;
}

/**
 * Helper function to process import items.
 *
 * @param $line
 *   Array which contains items of a cleaned and checked csv line.
 * @param $options
 *   Array of import options (same as taxonomy_csv_line_import):
 *     import_format  : see _taxonomy_csv_info_lists('list_import_format')
 *     vocabulary_id  : vocabulary id to import into
 *     existing_items : see _taxonomy_csv_info_lists('list_import_option')
 *     relations_create_subrelations: boolean. Specific to related terms import
 *     relations_all_vocabularies   : boolean. Id
 * @param $previous_items
 *   Optional. Cleaned and checked previous imported line names and tids array.
 *   Needed with some contents as one term array structure import or taxonomy manager export.
 *   With taxonomy manager export, tid array is indexed with old tids.
 * @return Result array:
 *   'name' => array of imported terms names,
 *   'tid'  => array of imported terms tids,
 *   'msg'  => messages arrays:
 *     term position => array of status messages of term.
 */
function _taxonomy_csv_line_import_process($line, $options, $previous_items = array()) {
  // Define default values.
  $result = array(
    'name' => array(),
    'tid'  => array(),
    'msg'  => array(),
  );

  // Only count check because line and previous line already checked.
  if (count($line)) {
    switch ($options['import_format']) {
      case TAXONOMY_CSV_FORMAT_FLAT:
        foreach ($line as $term_name) {
          $term = array(
            'name' => $term_name,
            'vid'  => $options['vocabulary_id'],
          );
          // Import term then store and check result.
          $current_result = taxonomy_csv_term_import($term, $options['existing_items']);
          $result['name'][] = $current_result['name'];
          $result['tid'][]  = $current_result['tid'];
          $result['msg'][]  = $current_result['msg'];
          if (_taxonomy_csv_message_warning($current_result['msg'])) {
            break;
          }
        }
        break;

      case TAXONOMY_CSV_FORMAT_TREE_STRUCTURE:
        // Same process, except when term is imported.
      case TAXONOMY_CSV_FORMAT_POLYHIERARCHY:
        // First, complete line with previous line.
        // Calculate first non empty item as line can be a full, partial or one term array.
        for ($first_non_empty = 0; ($first_non_empty < count($line)) && (empty($line[$first_non_empty])); $first_non_empty++) {
        }

        // Clean line of surabondant previous items then memorize previous terms for next line.
        if ($first_non_empty) {
          $result['name'] = $previous_items['name'] = array_slice($previous_items['name'], 0, $first_non_empty);
          $result['tid']  = $previous_items['tid']  = array_slice($previous_items['tid'], 0, $first_non_empty);
          foreach ($previous_items['tid'] as $key => $term) {
            $result['msg'][]  = array(FALSE); // No error: Previous line term.
          }
        }

        // Second, process import of new items.
        // Set root or previous ancestor name and id.
        $parent_name = ($first_non_empty == 0) ? '' : $previous_items['name'][$first_non_empty - 1];
        $parent_tid  = ($first_non_empty == 0) ? 0 : $previous_items['tid'][$first_non_empty - 1];

        for ($c = $first_non_empty; $c < count($line); $c++) {
          $term = array(
            'name'   => $line[$c],
            'vid'    => $options['vocabulary_id'],
            // Need an array in case of multiple parent.
            'parent' => array($parent_tid),
          );

          // Import term then store and check result.
          switch ($options['import_format']) {
            case TAXONOMY_CSV_FORMAT_TREE_STRUCTURE:
              // With TAXONOMY_CSV_EXISTING_IGNORE_CREATE, parent terms (so all terms but the last on this line)
              // are always updated because they are successive parents of a child.
              $current_result = ($options['existing_items'] == TAXONOMY_CSV_EXISTING_IGNORE_CREATE && ($c < count($line) - 1)) ?
                  taxonomy_csv_term_import($term, TAXONOMY_CSV_EXISTING_IGNORE_PREVIOUS, FALSE, $parent_tid) :
                  taxonomy_csv_term_import($term, $options['existing_items'], FALSE, $parent_tid);
              break;

            case TAXONOMY_CSV_FORMAT_POLYHIERARCHY:
              // Manage eventual direct duplicates: in Drupal, a term can't be a parent of itself.
              $current_result = ($term['name'] == $parent_name) ?
                  taxonomy_csv_term_import($term, $options['existing_items'], FALSE, $parent_tid) :
                  taxonomy_csv_term_import($term, $options['existing_items'], FALSE, NULL);
              break;
          }
          $result['name'][] = $current_result['name'];
          $result['tid'][]  = $current_result['tid'];
          $result['msg'][]  = $current_result['msg'];
          if (_taxonomy_csv_message_warning($current_result['msg'])) {
            break;
          }

          $parent_name = $current_result['name'];
          $parent_tid  = $current_result['tid'];
        }
        break;

      case TAXONOMY_CSV_FORMAT_PARENTS:
        // First, import parents using a recursive call and check result.
        if (count($line) > 1) {
          $options_temp = $options;
          $options_temp['import_format'] = TAXONOMY_CSV_FORMAT_FLAT;
          $result = _taxonomy_csv_line_import_process(array_slice($line, 1), $options_temp);
          if (_taxonomy_csv_message_warning($result, 'line')) {
            break;
          }
        }

        // Second,import main term then store result. No check because only one term.
        // No need to check if tid is equal to parent_tid, because duplicates have been removed.
        $term = array(
          'name'   => $line[0],
          'vid'    => $options['vocabulary_id'],
          'parent' => $result['tid'],
        );
        $current_result = taxonomy_csv_term_import($term, $options['existing_items']);
        // Use array_unshift in order to keep $line order of items.
        array_unshift($result['name'], $current_result['name']);
        array_unshift($result['tid'], $current_result['tid']);
        array_unshift($result['msg'], $current_result['msg']);
        break;

      case TAXONOMY_CSV_FORMAT_CHILDREN:
        // First, import main term then store and check result.
        $options_temp = $options;
        $options_temp['import_format'] = TAXONOMY_CSV_FORMAT_ALONE_TERMS;
        $result = _taxonomy_csv_line_import_process(array($line[0]), $options_temp);
        if (_taxonomy_csv_message_warning($result, 'line')) {
          break;
        }
        // Remember tid of main term.
        $parent_tid = $result['tid'][0];

        // Second, import children.
        foreach (array_slice($line, 1) as $value) {
          $term = array(
            'name'   => $value,
            'vid'    => $options['vocabulary_id'],
            'parent' => array($parent_tid),
          );

          // Import term then store and check result.
          // No need to check if tid is equal to parent_tid, because duplicates have been removed.
          $current_result = taxonomy_csv_term_import($term, $options['existing_items']);
          $result['name'][] = $current_result['name'];
          $result['tid'][]  = $current_result['tid'];
          $result['msg'][]  = $current_result['msg'];
          if (_taxonomy_csv_message_warning($current_result['msg'])) {
            break;
          }
        }
        break;

      case TAXONOMY_CSV_FORMAT_RELATIONS:
        // Each related term should exist before first column term can be related to it (need of its tid). Next, process relations and eventually subrelations
        // So, with a line array (A,B,C,D,E...), creates A, BA, CA(B), DA(BC), EA(BCD)... as it's the fatest way to create one or all relations even if items not exists.
        // As use of name is only for first column, need to keep previous tids matching names in $previous_relations ([name] <=> [tid]).
        $previous_relations = array();
        foreach ($line as $key => $term) {
          $term = array(
            'name'      => $line[$key],
            'vid'       => $options['vocabulary_id'],
            'relations' => array_slice($previous_relations, 0, ($options['relations_create_subrelations']) ? $key : $key != 0),
          );

          ///TODO: Set an 'ignore all' option.
          // For second and next terms, update merge related terms in order to keep previous relations, as option is only for the first column term.
          $current_term_import_option = ($key == 0) ? $options['existing_items'] : TAXONOMY_CSV_EXISTING_UPDATE_MERGE;

          $current_result = taxonomy_csv_term_import($term, $current_term_import_option, $options['relations_all_vocabularies'], NULL);
          $result['name'][] = $current_result['name'];
          $result['tid'][]  = $current_result['tid'];
          $result['msg'][]  = $current_result['msg'];
          if (_taxonomy_csv_message_warning($current_result['msg'])) {
            break;
          }

          $previous_relations[$current_result['name']] = $current_result['tid'];
        }
        break;

      case TAXONOMY_CSV_FORMAT_TAXONOMY_MANAGER:
        $term = array(
          'name'        => $line[2],
          'vid'         => $options['vocabulary_id'],
        );
        if ($line[3]) {
          $term['description'] = $line[3];
        }

        // Complete with parents except root. All parents exist, because it's previously checked.
        $list_parents = array_slice($line, 4);
        if ($list_parents[0] != 0) {
          foreach ($list_parents as $value) {
            $term['parent'][] = $previous_items['tid'][$value];
          }
        }

        // Use tid if term is already imported.
        if (isset($previous_items['tid'][$line[1]])) {
          $term['tid'] = $previous_items['tid'][$line[1]];
        }
        else {
          // No tid is given, so it's always a new term.
          $options['existing_items'] = TAXONOMY_CSV_EXISTING_IGNORE_CREATE;
        }

        // Import term then store result. No check because only one term.
        $current_result = taxonomy_csv_term_import($term, $options['existing_items']);
        // Keep old index (names and tids).
        $result = $previous_items;
        $result['name'][$line[1]] = $current_result['name'];
        $result['tid'][$line[1]]  = $current_result['tid'];
        $result['msg'][$line[1]]  = $current_result['msg'];
        break;

      case TAXONOMY_CSV_FORMAT_ALONE_TERMS:
      case TAXONOMY_CSV_FORMAT_FIELDS:
      case TAXONOMY_CSV_FORMAT_DESCRIPTIONS:
      case TAXONOMY_CSV_FORMAT_WEIGHTS:
      case TAXONOMY_CSV_FORMAT_SYNONYMS:
        $term = array(
          'name' => $line[0],
          'vid'  => $options['vocabulary_id'],
        );
        switch ($options['import_format']) {
          case TAXONOMY_CSV_FORMAT_FIELDS:
            $term['weight']      = $line[1];
            $term['description'] = $line[2];
            $term['synonyms']    = array_slice($line, 3);
            break;

          case TAXONOMY_CSV_FORMAT_DESCRIPTIONS:
            $term['description'] = $line[1];
            break;

          case TAXONOMY_CSV_FORMAT_WEIGHTS:
            $term['weight'] = $line[1];
            break;

          case TAXONOMY_CSV_FORMAT_SYNONYMS:
            $term['synonyms'] = array_slice($line, 1);
            break;
        }
        // Import term then store result. No check because only one term.
        $current_result = taxonomy_csv_term_import($term, $options['existing_items']);
        $result['name'][] = $current_result['name'];
        $result['tid'][]  = $current_result['tid'];
        $result['msg'][]  = $current_result['msg'];
        break;

      default:
        $result['msg'][] = array(TRUE); // Error unknown source content.
    }
  }
  else {
    $result['msg'][] = array(TRUE); // No term to process.
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
 * @param $existing_items
 *   Optional. Type of import on existing terms. Default to ignore and create.
 * @param $all_vocabularies
 *   Optional. Boolean. Search in all vocabularies
 *   or only in $term['vid'] vocabulary (default), which need to be set.
 * @param $parent_tid
 *   Optional. Boolean. Restrict search in parent ($term['parent'][0] and next).
 *   Useful for structure. Default to NULL (search in whole vocabulary).
 * @return array
 *    'name' => term name,
 *    'tid'  => term id,
 *    'msg'  => messages array.
 */
function taxonomy_csv_term_import($term, $existing_items = TAXONOMY_CSV_EXISTING_IGNORE_CREATE, $all_vocabularies = FALSE, $parent_tid = NULL) {
  $messages = array();

  if ($term) {
    switch ($existing_items) {
      case TAXONOMY_CSV_EXISTING_UPDATE:
        // Equal to 'update_merge' or 'update_replace'.
      case TAXONOMY_CSV_EXISTING_UPDATE_MERGE:
        $existing_term = _taxonomy_csv_term_find_existing($term, $all_vocabularies, $parent_tid);
        if ($existing_term) {
          // Needed because get name is not case sensitive.
          if (isset($term['name'])) {
            $existing_term['name'] = $term['name'];
          }
          if (isset($term['vid'])) {
            $existing_term['vid'] = $term['vid'];
          }
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
            $i = trim($existing_term['description']);
            $existing_term['description'] = (($i == '') || ($i == $term['description'])) ?
                $term['description'] :
                $i ."\n". $term['description'];
          }
          // Weight is always replaced as it is a simple number.
          if (isset($term['weight'])) {
            $existing_term['weight'] = $term['weight'];
          }
          $term = $existing_term;
        }
        break;

      case TAXONOMY_CSV_EXISTING_IGNORE_PREVIOUS:
        //Doesn't ignore, but use previous parents. De facto equal to update_replace.
      case TAXONOMY_CSV_EXISTING_UPDATE_REPLACE:
        $existing_term = _taxonomy_csv_term_find_existing($term, $all_vocabularies, $parent_tid);
        if ($existing_term) {
          foreach (array(
              'name',   // Needed because get name is not case sensitive.
              'vid',
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
        // Equal to 'ignore_create'.
      case TAXONOMY_CSV_EXISTING_IGNORE_CREATE:
        break;

      case TAXONOMY_CSV_EXISTING_IGNORE_ALL:
      ///TODO: IGNORE_ALL
    }

    // If a term has no parent, it's a root term, so adds parent 0.
    if (!isset($term['parent'])
        || count($term['parent']) == 0) {
      $term['parent'] = array(0);
    }
    // If a term has more than one parent, it can't be a root term, so removes parent 0.
    elseif ((count($term['parent']) > 1)
        && in_array(0, $term['parent'])) {
      unset($term['parent'][array_search(0, $term['parent'])]);
    }

    $result = taxonomy_csv_term_save($term);

    $messages[] = FALSE; // No error.
  }
  else {
    $messages[] = TRUE; // Error: not a term.
  }
  return array(
    'name' => $term['name'],
    'tid'  => $term['tid'],
    'msg'  => $messages,
  );
}

/**
 * Find an existing term in vocabularies by its tid else by its name.
 *
 * @param $term
 *   The term array to find.
 * @param $all_vocabularies
 *   Optional. Boolean. Search in all vocabularies
 *   or only in $term['vid'] vocabulary (default), which need to be set.
 * @param $parent_tid
 *   Optional. The direct parent term id.
 *   Useful for structure. Default to NULL (no parent restriction).
 * @return
 *   Found term array or empty array if not found.
 */
function _taxonomy_csv_term_find_existing($term, $all_vocabularies = FALSE, $parent_tid = NULL) {
  if (!empty($term['tid'])) {
    return taxonomy_csv_term_get($term['tid']);
  }
  elseif (isset($term['name'])) {
    return taxonomy_csv_term_find($term['name'], ($all_vocabularies) ? NULL : $term['vid'], $parent_tid);
  }
  // ERROR: no tid neither name.
  return FALSE;
}

/**
 * Get a full term by its id.
 *
 * @param $tid
 *   The term id to get.
 * @return
 *   Found term array as describe in taxonomy_csv_term_import or an empty array.
 */
function taxonomy_csv_term_get($tid) {
  $term = (array) taxonomy_get_term($tid);
  if ($term) {
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
 *   Optional. The direct parent term id if any. Default to NULL (none).
 * @return
 *   Found term array as describe in taxonomy_csv_term_import or an empty array.
 */
function taxonomy_csv_term_find($name, $vid = NULL, $parent_tid = NULL) {
  $name = drupal_strtolower(trim($name));

  if (drupal_strlen($name)) {
    $sql = "
      SELECT t.*, h.parent
      FROM {term_data} t
      INNER JOIN {term_hierarchy} h ON t.tid = h.tid
      WHERE '%s' LIKE LOWER(t.name)
    ";
    $args = array($name);

    if ($vid) {
      $sql .= ' AND t.vid = %d';
      $args[] = $vid;
    }

    if ($parent_tid) {
      $sql .= ' AND h.parent = %d';
      $args[] = $parent_tid;
    }

    $sql .= ' ORDER BY {tid} ASC LIMIT 1';

    $result = db_query($sql, $args);
    $term   = db_fetch_array($result);

    if ($term) {
      $term['parent']    = taxonomy_csv_term_get_parents_tids($term['tid']);
      $term['relations'] = taxonomy_csv_term_get_related_tids($term['tid']);
      $term['synonyms']  = taxonomy_get_synonyms($term['tid']);

      return $term;
    }
  }
  return array();
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
 * If no parent, return array(0), because a term without parent is a root term.
 * If tid doesn't exist, return array(0) too (not checked).
 * Return empty array only if tid is not set.
 */
function taxonomy_csv_term_get_parents_tids($tid) {
  $parents_tids = array();
  if (!empty($tid)) {
    $parents_terms = taxonomy_get_parents($tid);
    if ($parents_terms) {
      foreach ($parents_terms as $term => $item) {
        $parents_tids[] = $parents_terms[$term]->tid;
      }
      return $parents_tids;
    }
    return array(0);
  }
  return array();
}

/**
 * Return an array of all term IDs related to a given term ID.
 */
function taxonomy_csv_term_get_related_tids($tid) {
  $related_tids = array();
  if (!empty($tid)) {
    $related_terms = taxonomy_get_related($tid);
    if ($related_terms) {
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
 * @defgroup taxonomy_csv_message Messages helpers
 * @{
 * Functions managing import messages.
 */

/**
 * Helper which returns a boolean when a set of message codes contains a warning or worst.
 *
 * @param $messages
 *   Array of message code (TRUE if any error or FALSE if none).
 * @param $type
 *   Type of messages array:
 *     'msg' (default), 'line'.
 * @return
 *   TRUE if warning or error, else FALSE if no issue.
 */
function _taxonomy_csv_message_warning($messages, $type = 'msg') {
  switch ($type) {
    case 'msg':
      return in_array(TRUE, $messages, TRUE);

    case 'line':
      $list_messages = array();
      foreach ($messages['msg'] as $message_codes) {
        $list_messages = array_merge($list_messages, $message_codes);
      }
      return in_array(TRUE, $list_messages, TRUE);
  }
  return TRUE;
}

/**
 * @} End of "defgroup taxonomy_csv_message".
 */

/**
 * @defgroup taxonomy_csv_info Informations helpers.
 * @{
 * Functions informing about import process.
 */

/**
 * Helper to remember some items.
 *
 * @param $list
 *   A string matching list to be returned:
 *     'list_import_format',
 *     'list_import_option',
 *     'list_import_format_allowed_import_option',
 * @return wanted content.
 */
function _taxonomy_csv_info_lists($list) {
  switch ($list) {
    case 'list_import_format':
      // Some import types will be enabled in a next release.
      return array(
        TAXONOMY_CSV_FORMAT_ALONE_TERMS      => t('Term names (ignore additional columns)'),
        TAXONOMY_CSV_FORMAT_FLAT             => t('Terms (flat vocabulary)'),
        TAXONOMY_CSV_FORMAT_TREE_STRUCTURE   => t('Hierarchical tree structure or one term by line structure'),
        TAXONOMY_CSV_FORMAT_POLYHIERARCHY    => t('Polyhierarchical structure'),
        TAXONOMY_CSV_FORMAT_PARENTS          => t('First level parents'),
        TAXONOMY_CSV_FORMAT_CHILDREN         => t('First level children'),
        TAXONOMY_CSV_FORMAT_RELATIONS        => t('Related terms'),
        TAXONOMY_CSV_FORMAT_FIELDS           => t('Full term definitions'),
        TAXONOMY_CSV_FORMAT_DESCRIPTIONS     => t('Term descriptions'),
        TAXONOMY_CSV_FORMAT_WEIGHTS          => t('Term weights'),
        TAXONOMY_CSV_FORMAT_SYNONYMS         => t('Synonyms terms'),
        TAXONOMY_CSV_FORMAT_TAXONOMY_MANAGER => t('Taxonomy manager'),
      );

    case 'list_import_option':
      return array(
        TAXONOMY_CSV_EXISTING_UPDATE         => t('Update existing term'),
        TAXONOMY_CSV_EXISTING_UPDATE_MERGE   => t('Update (merge) existing term and items if exist else create'),
        TAXONOMY_CSV_EXISTING_UPDATE_REPLACE => t('Update (replace) existing term and items if exist else create'),
        TAXONOMY_CSV_EXISTING_IGNORE         => t('Duplicate existing term'),
        TAXONOMY_CSV_EXISTING_IGNORE_CREATE  => t('Ignore existing term and create a new term'),
        TAXONOMY_CSV_EXISTING_IGNORE_ALL     => t('Ignore existing term and create a new term for each term on the line'),
      );

    case 'list_import_format_allowed_import_option':
      return array(
        TAXONOMY_CSV_FORMAT_ALONE_TERMS      => array(
          TAXONOMY_CSV_EXISTING_UPDATE,
          TAXONOMY_CSV_EXISTING_IGNORE,
        ),
        TAXONOMY_CSV_FORMAT_FLAT             => array(
          TAXONOMY_CSV_EXISTING_UPDATE,
          TAXONOMY_CSV_EXISTING_IGNORE,
        ),
        TAXONOMY_CSV_FORMAT_TREE_STRUCTURE   => array(
          TAXONOMY_CSV_EXISTING_UPDATE_REPLACE,
          TAXONOMY_CSV_EXISTING_IGNORE_CREATE,
//           TAXONOMY_CSV_EXISTING_IGNORE_ALL,
        ),
        TAXONOMY_CSV_FORMAT_POLYHIERARCHY    => array(
          TAXONOMY_CSV_EXISTING_UPDATE_MERGE,
        ),
        TAXONOMY_CSV_FORMAT_PARENTS          => array(
          TAXONOMY_CSV_EXISTING_UPDATE_MERGE,
          TAXONOMY_CSV_EXISTING_UPDATE_REPLACE,
          TAXONOMY_CSV_EXISTING_IGNORE_CREATE,
//           TAXONOMY_CSV_EXISTING_IGNORE_ALL,
        ),
        TAXONOMY_CSV_FORMAT_CHILDREN         => array(
          TAXONOMY_CSV_EXISTING_UPDATE_MERGE,
          TAXONOMY_CSV_EXISTING_UPDATE_REPLACE,
          TAXONOMY_CSV_EXISTING_IGNORE_CREATE,
//           TAXONOMY_CSV_EXISTING_IGNORE_ALL,
        ),
        TAXONOMY_CSV_FORMAT_RELATIONS        => array(
          TAXONOMY_CSV_EXISTING_UPDATE_MERGE,
          TAXONOMY_CSV_EXISTING_UPDATE_REPLACE,
          TAXONOMY_CSV_EXISTING_IGNORE_CREATE,
//           TAXONOMY_CSV_EXISTING_IGNORE_ALL,
        ),
        TAXONOMY_CSV_FORMAT_FIELDS           => array(
          TAXONOMY_CSV_EXISTING_UPDATE_MERGE,
          TAXONOMY_CSV_EXISTING_UPDATE_REPLACE,
          TAXONOMY_CSV_EXISTING_IGNORE_CREATE,
        ),
        TAXONOMY_CSV_FORMAT_DESCRIPTIONS     => array(
          TAXONOMY_CSV_EXISTING_UPDATE_MERGE,
          TAXONOMY_CSV_EXISTING_UPDATE_REPLACE,
          TAXONOMY_CSV_EXISTING_IGNORE_CREATE,
        ),
        TAXONOMY_CSV_FORMAT_WEIGHTS          => array(
          TAXONOMY_CSV_EXISTING_UPDATE_MERGE,
          TAXONOMY_CSV_EXISTING_IGNORE_CREATE,
        ),
        TAXONOMY_CSV_FORMAT_SYNONYMS         => array(
          TAXONOMY_CSV_EXISTING_UPDATE_MERGE,
          TAXONOMY_CSV_EXISTING_UPDATE_REPLACE,
          TAXONOMY_CSV_EXISTING_IGNORE_CREATE,
        ),
        TAXONOMY_CSV_FORMAT_TAXONOMY_MANAGER => array(
          TAXONOMY_CSV_EXISTING_UPDATE_MERGE,
          TAXONOMY_CSV_EXISTING_UPDATE_REPLACE,
          TAXONOMY_CSV_EXISTING_IGNORE_CREATE,
        ),
      );
  }
}

/**
 * @} End of "defgroup taxonomy_csv_info".
 */
