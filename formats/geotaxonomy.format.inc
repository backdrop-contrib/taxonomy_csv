<?php
// $Id$

/**
 * @file
 *   Plug-in that manages geotaxonomy vocabularies import/export.
 *
 * @see http://drupal.org/project/geotaxonomy
 */

/**
 * Return infos on the format.
 */
function taxonomy_csv_format_geotaxonomy() {
  return array(
    'format'         => 'geotaxonomy',
    'name'           => t('Geotaxonomy'),
    'needed_module'  => 'geotaxonomy',
    'import_format'  => t('Geotaxonomy'),
    'export_format'  => t('Geotaxonomy'),
    'import_allowed' => array(
      TAXONOMY_CSV_EXISTING_UPDATE_MERGE,
      TAXONOMY_CSV_EXISTING_UPDATE_REPLACE,
      TAXONOMY_CSV_EXISTING_IGNORE_CREATE,
    ),
    'import_previous'=> TRUE, // Because import remembers all previous_items.
    'specific_fields'=> TRUE,
    'description'    => t('Import <a href="!link">geotaxonomy</a> terms.', array(
      '!link' => url('http://drupal.org/project/geotaxonomy'),
    )),
    'description_format'  => t('name, unique id, description, parent, parent unique id, weight, latitude, longitude, top, right, bottom, left boundaries'),
    'description_example' => t('"Paris", , "Capital of France", "Île-de-France", , 5, 48, 2, 10, 11, 12, 13') . '<br />'
      . t('"Île-de-France", , "Main region", "France", , 3, 50, 0, 100, 110, 120, 130'),
    'description_long'    => t('Only name and either latitude or longitude are needed. To set a parent is recommended to get a hierarchical structure.') . '<br />'
      . t('Ids may be numbers or not, for example the name itself, but they need to be unique to avoid duplicate terms. Only duplicate names need to have an id.'),
    'import_options_help' => t('"Merge" and "Replace" are very similar, because geotaxonomy items are unique: there are only one latitude, longitude, etc. Difference is for parents. With "Merge", existing parents will be kept. With "Replace", they will be removed.'),
  );
}

/**
 * Check if a vocabulary is a geotaxonomy one.
 *
 * @param $vid
 *   Vocabulary id to check.
 *
 * @return
 *   TRUE if vocabulary has specific fields, FALSE else.
 */
function taxonomy_csv_vocabulary_check_geotaxonomy($vid) {
  $vocabulary_settings = variable_get('geotaxonomy_vocabularies', array());
  return (isset($vocabulary_settings[$vid]) && ($vocabulary_settings[$vid] == 1));
}

/**
 * Add option to use geotaxonomy with vocabulary. Wrapper of geotaxonomy hook.
 *
 * @param $vocabulary
 *   Vocabulary object to update.
 *
 * @return
 *   Nothing because hook return nothing.
 */
function _taxonomy_csv_import_vocabulary_prepare_geotaxonomy($vocabulary) {
  // Needed because form_state is a by reference variable.
  $values = array('values' => array(
    'vid'                => $vocabulary->vid,
    'geotaxonomy_status' => 1,
  ));

  $result = geotaxonomy_form_vocabulary_submit(
    array(),
    $values
  );
}

/**
 * Check items of an imported line.
 */
function _taxonomy_csv_line_import_check_geotaxonomy($line, $options, $previous_items = array(), &$messages = array()) {
  $checked_items = $line;

  switch ($checked_items) { // Simulate a goto with break.
    default:
      if (empty($line[0])) {
        $messages[] = 480; // Warning no first column.
        break;
      }
      if (count($line) < 7) {
        $messages[] = 450; // Warning some items lacks.
        break;
      }
      if (!is_numeric($line[5])) {
        $messages[] = 451; // Warning item are not numbers.
        break;
      }
      if (empty($line[6])
          && (!isset($line[7]) || empty($line[7]))) {
        $messages[] = 482; // Warning no item in some columns.
        break;
      }
      if (!is_numeric($line[6])
          && (!isset($line[7]) || !is_numeric($line[7]))) {
        $messages[] = 451; // Warning some items are not numbers.
        break;
      }
      foreach (array($line[0], $line[1], $line[3], $line[4]) as $name) {
        if (drupal_strlen($name) > 255) {
          $messages[] = 454; // Warning too long.
          break 2;
        }
      }
      // @todo To be confirmed.
      if (($line[0] == $line[3])
          && ($line[1] == $line[4])) {
        $messages[] = 452; // Warning a term can't be a parent of itself.
        break;
      }
      if (count($line) > 12) {
        $messages[] = 564; // Notice too many items.
        $checked_items = array_slice($line, 12);
      }
      // Clean line of non numeric values.
      foreach ($checked_items as $key => $value) {
        if ($key > 5) {
          $checked_items[$key] = is_numeric($value) ?
            $value :
            NULL;
         }
      }
  }

  return array_values($checked_items);
}

/**
 * Import a line of items.
 *
 * @param $previous_items
 *   Specificity: tid and name arrays are indexed with unique ids, and with name
 *   when unique id is not defined.
 */
function taxonomy_csv_line_import_geotaxonomy($line, $options, $previous_items = array()) {
  // Define default values.
  $result = array(
    'name' => array(),
    'tid'  => array(),
    'msg'  => array(),
  );

  switch ($result) { // Simulate a goto with break.
    default:
      // Keep index (names and tids) with unique id.
      $result['name'] = $previous_items['name'];
      $result['tid']  = $previous_items['tid'];

      // First, import parent if needed using a recursive call and check result.
      if (!empty($line[3])) {
        $parent_unique_id = empty($line[4]) ? $line[3] : $line[4];

        // Import a new term if it is not already imported.
        if (!isset($previous_items['tid'][$parent_unique_id])) {
          $term = (object) array(
            'name' => $line[3],
            'vid'  => $options['vocabulary_id'],
          );
          $current_result = taxonomy_csv_term_import($term, $options['existing_items']);
          $result['name'][$parent_unique_id] = $current_result['name'];
          $result['tid'][$parent_unique_id]  = $current_result['tid'];
          $result['msg'] = $current_result['msg'];
          if (_taxonomy_csv_worst_message($result['msg']) < TAXONOMY_CSV_PROCESS_NOTICE) {
            break;
          }
        }
        $parent_name = $result['name'][$parent_unique_id];
        $parent_tid  = $result['tid'][$parent_unique_id];
      }

      // Second, import (save or update) main term.
      $unique_id = empty($line[1]) ? $line[0] : $line[1];

      // Complete line with NULL if needed.
      $line = array_pad($line, 12, NULL);

      $term = (object) array(
        'name'        => $line[0],
        'vid'         => $options['vocabulary_id'],
        'description' => $line[2],
        'weight'      => $line[5],
        'parents'     => array(isset($parent_tid) ? $parent_tid : 0),
      // Geotaxonomy specific fields.
        'lat'         => $line[6],
        'lon'         => $line[7],
        'bound_top'   => $line[8],
        'bound_right' => $line[9],
        'bound_bottom'=> $line[10],
        'bound_left'  => $line[11],
      );
      // Add unique ids only if they are set.
      if (isset($result['tid'][$unique_id])) {
        $term->tid = $result['tid'][$unique_id];
      }
      if (isset($line[1])) {
        $term->external_id = $line[1];
      }
      if (isset($line[3])) {
        $term->parent_name = $line[3];
      }
      if (isset($line[4])) {
        $term->parent_external_id = $line[4];
      }

      // Import term then store result. No check because only one term.
      $current_result = taxonomy_csv_term_import($term, $options['existing_items'], FALSE, '', $options['import_format']);
      $result['name'][$unique_id] = $current_result['name'];
      $result['tid'][$unique_id]  = $current_result['tid'];
      $result['msg'] = array_merge($result['msg'], $current_result['msg']);
  }

  return $result;
}

/**
 * Save specific fields of a term and only them.
 *
 * @param $term
 *   Term object to be imported.
 *
 * @return
 *   Nothing because geotaxonomy_taxonomy() return nothing.
 */
function taxonomy_csv_term_import_geotaxonomy($term) {
  return geotaxonomy_taxonomy('update', 'term', (array) $term);
}

/**
 * Export a term.
 */
function taxonomy_csv_term_export_geotaxonomy($term, $options = array(), $duplicate_terms = array()) {
  // Define default values.
  $result = array(
    'line' => array(),
    'msg'  => array(),
  );

  // Get extra fields.
  $fields = taxonomy_csv_term_get_geotaxonomy($term->tid);

  // Doesn't manage duplicate terms because two specific fields do this.
  $result['line'] = array(
    $term->name,
    $fields['external_id'],
    $term->description,
    $fields['parent_name'],
    $fields['parent_external_id'],
    $term->weight,
    $fields['lat'],
    $fields['lon'],
    $fields['bound_top'],
    $fields['bound_right'],
    $fields['bound_bottom'],
    $fields['bound_left'],
  );

  return $result;
}

/**
 * Get full term with specific geotaxonomy fields from a term id.
 *
 * @param $tid
 *   The term id to get.
 *
 * @return
 *   Full term object with specific fields.
 */
function taxonomy_csv_term_load_geotaxonomy($tid) {
  $term = taxonomy_csv_term_load($tid);
  return ($term) ?
    taxonomy_csv_term_get_full_geotaxonomy($term) :
    FALSE;
}

/**
 * Complete a term with geotaxonomy fields.
 *
 * @param $term
 *   The term object to complete.
 *
 * @return
 *   Full term object with specific fields if any.
 */
function taxonomy_csv_term_get_full_geotaxonomy($term) {
  if ($term->tid) {
    $fields = taxonomy_csv_term_get_geotaxonomy($term->tid);

    return ($fields) ?
      (object) array_merge((array) $term, $fields) :
      $term;
  }
}

/**
 * Get specific fields of a geotaxonomy term.
 *
 * geotaxonomy_get_term($tid) doesn't return all fieds (parent_name...) and
 * empty fields aren't returned.
 *
 * @param $tid
 *   The term id to get.
 *
 * @return
 *   Specific fields array.
 */
function taxonomy_csv_term_get_geotaxonomy($tid) {
  return db_fetch_array(db_query("SELECT lat, lon, bound_top, bound_right, bound_bottom, bound_left,  geometry, external_id, parent_name, parent_external_id FROM {term_geo} WHERE tid = %d", $tid));
}
