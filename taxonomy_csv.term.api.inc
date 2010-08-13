<?php
// $Id$

/**
 * @file
 * Find, get and set full or detail term items.
 */

/**
 * Find and load a term.
 *
 * @todo Use a regular Drupal 7 dbquery.
 *
 * Need to maintain a specific function and a direct query, because
 * taxonomy_term_load_multiple doesn't manage parents.
 *
 * @param $term
 *   The term object to find. It's not necessarily a standard term object. It's
 *   an object which needs only a name and eventually a vid or a parent id. Of
 *   course, if tid is set, the found term is existing one.
 * @param $all_vocabularies
 *   Optional. Boolean. Search in all vocabularies or only in $term->vid
 *   vocabulary (default), which need to be set. Used with relations import.
 * @param $parent_tid
 *   Optional. The direct parent term id where to restrict search.
 *   Used for structure import. Default to NULL (no parent restriction).
 *
 * @return
 *   Formatted found term object, or FALSE if not found or error.
 */
function taxonomy_csv_term_find($term, $all_vocabularies = FALSE, $parent_tid = NULL) {
  if (isset($term->tid) && $term->tid) {
    $found_term = taxonomy_term_load($term->tid);
  }
  elseif (isset($term->name)) {
    $name = drupal_strtolower(trim($term->name));

    if (drupal_strlen($name)) {
      // Only term id is selected, because taxonomy_term_load is used next in
      // order to take advantage of taxonomy cache.
      $sql = "
        SELECT t.tid
        FROM {taxonomy_term_data} t
        INNER JOIN {taxonomy_term_hierarchy} h ON t.tid = h.tid
        WHERE :name LIKE LOWER(t.name)
      ";
      $args = array();
      $args[':name'] = $name;

      if (isset($term->vid)
          && $term->vid
          && !$all_vocabularies) {
        $sql .= ' AND {t.vid} = :vid';
        $args[':vid'] = $term->vid;
      }

      if ($parent_tid) {
        $sql .= ' AND {h.parent} = :parent';
        $args[':parent'] = $parent_tid;
      }

      $sql .= ' ORDER BY {t.tid} ASC LIMIT 1';

      $result = db_query($sql, $args);
      // Only zero or one result.
      foreach ($result as $item) {
        $found_term = taxonomy_term_load($item->tid);
      }
    }
  }

  // Complete and internal format term.
  if (isset($found_term)) {
    $found_term->parents = taxonomy_csv_term_get_parents_tids($found_term->tid);
    return _taxonomy_csv_term_format_internal($found_term);
  }

  // Not found, or error (neither tid nor name).
  return FALSE;
}

/**
 * Return an array of all parents term IDs of a given term id.
 */
function taxonomy_csv_term_get_parents_tids($tid) {
  return array_keys(taxonomy_get_parents($tid));
}

/**
 * Return an array of all parents term names of a given term id.
 */
function taxonomy_csv_term_get_parents_names($tid) {
  return array_keys(taxonomy_get_parents($tid, 'name'));
}

/**
 * Return an array of all children term id of a given term id.
 */
function taxonomy_csv_term_get_children_tids($tid) {
  return array_keys(taxonomy_get_children($tid));
}

/**
 * Return an array of all children term names of a given term id.
 */
function taxonomy_csv_term_get_children_names($tid) {
  return array_keys(taxonomy_get_children($tid, 0, 'name'));
}

/**
 * Return an array of values of a field of a given term id or object.
 *
 * @todo To be removed by using of field api.
 *
 * Values can be names or ids depending on field type (text or term reference).
 *
 * @param $term_tid
 *   Term object or term id from which to get values. Use of object is quicker.
 * @param $field_name
 *   Field to get.
 * @param $langcode
 *   Optional. Language code to use.
 *
 * @return
 *   Result array of items.
 */
function taxonomy_csv_term_get_field_values($term_tid, $field_name, $langcode = 'und') {
  $items = array();

  $term = is_object($term_tid) ? $term_tid : taxonomy_term_load($term_tid);

  $value = ($field_name == 'taxonomy_relation') ?
    'tid' :
    'value';

  if (isset($term->{$field_name}[$langcode])) {
    foreach ($term->{$field_name}[$langcode] as $delta) {
      $items[] = $delta[$value];
    }
  }

  return $items;
}

/**
 * Return an array of all term objects of a reference field of a given term ID.
 *
 * @param $term_tid
 *   Term object or term id from which to get term objects. Use of object is
 *   quicker.
 * @param $field_name
 *   Term reference field to get.
 *
 * @return
 *   Result array of term objects.
 */
function taxonomy_csv_term_get_field_terms($term_tid, $field_name) {
  $items = array();

  $values = taxonomy_csv_term_get_field_values($term_tid, $field_name);

  foreach ($values as $tid) {
    $items[] = taxonomy_term_load($tid);
  }

  return $items;
}

/**
 * Return an array of all names of a term reference field of a given term ID.
 *
 * @param $term_tid
 *   Term object or term id from which to get names. Use of object is quicker.
 * @param $field_name
 *   Term reference field to get.
 *
 * @return
 *   Result array of items.
 */
function taxonomy_csv_term_get_field_names($term_tid, $field_name) {
  $items = array();

  $values = taxonomy_csv_term_get_field_values($term_tid, $field_name);

  foreach ($values as $tid) {
    $term = taxonomy_term_load($tid);
    $items[] = $term->name;
  }

  return $items;
}

/**
 * Return an array of all synonyms of a given term id or object.
 *
 * A synonym is not a term, so it hasn't a tid, but only a name. It's a text
 * field and not a term reference.
 * Wrapper of taxonomy_csv_term_get_field_values.
 */
function taxonomy_csv_term_get_synonyms($term_tid) {
  return taxonomy_csv_term_get_field_values($term_tid, 'taxonomy_synonym');
}

/**
 * Return an array of all related term ids of a given term id or object.
 * Wrapper of taxonomy_csv_term_get_field_values.
 */
function taxonomy_csv_term_get_relations($term_tid) {
  return taxonomy_csv_term_get_field_terms($term_tid, 'taxonomy_relation');
}

/**
 * Return an array of all related term ids of a given term id or object.
 * Wrapper of taxonomy_csv_term_get_field_values.
 */
function taxonomy_csv_term_get_relations_tids($term_tid) {
  return taxonomy_csv_term_get_field_values($term_tid, 'taxonomy_relation');
}

/**
 * Return an array of all related term names of a given term id or object.
 * Wrapper of taxonomy_csv_term_get_field_names.
 */
function taxonomy_csv_term_get_relations_names($term_tid) {
  return taxonomy_csv_term_get_field_names($term_tid, 'taxonomy_relation');
}

/**
 * Direct set values of a field of a given term id or object.
 *
 * @todo To be removed by using of field api.
 *
 * Values can be names or ids depending on field type (text or term reference).
 *
 * @param $term_tid
 *   Term object or term id on which to set values. Use of object is quicker.
 * @param $field_name
 *   Field to get.
 * @param $langcode
 *   Optional. Language code to use.
 *
 * @return
 *   Result array of items.
 */
function taxonomy_csv_term_set_field_values($term_tid, $field_name, $langcode = 'und') {
  $items = array();

  $term = is_object($term_tid) ? $term_tid : taxonomy_term_load($term_tid);

  $value = ($field_name == 'relations') ? 'tid' : 'value';

  if (isset($term->{$field_name})) {
    foreach ($term->{$field_name} as $delta) {
      $items[$langcode][] = array($value => $delta);
    }
  }

  return $items;
}

/**
 * Format a regular term for internal purpose (synonyms and relations).
 *
 * Use of real values is simpler to manage than field subarrays.
 *
 * @param $term
 *   The term object to format.
 *
 * @return
 *   Formatted term object.
 */
function _taxonomy_csv_term_format_internal($term) {
  if (isset($term->taxonomy_synonym)) {
    $term->synonyms = taxonomy_csv_term_get_field_values($term, 'taxonomy_synonym');
  }
  if (isset($term->taxonomy_relation)) {
    $term->relations = taxonomy_csv_term_get_field_values($term, 'taxonomy_relation');
  }

  return $term;
}

/**
 * Format an internal term to a regular term (synonyms and relations).
 *
 * Use of real values is simpler to manage than field subarrays.
 *
 * @param $term
 *   The term object to format.
 *
 * @return
 *   Formatted term object.
 */
function _taxonomy_csv_term_format_regular($term) {
  $langcode = 'und';

  if (isset($term->parents) && (count($term->parents) > 0)) {
    $term->parent = $term->parents;
    unset($term->parents);
  }
  else {
    $term->parent = array(0);
    unset($term->parents);
  }
  if (isset($term->synonyms)) {
    $field = taxonomy_csv_term_set_field_values($term, 'synonyms', $langcode);
    $term->taxonomy_synonym[$langcode] = ($field) ?
      array_shift($field) :
      array();
    unset($term->synonyms);
  }
  if (isset($term->relations)) {
    $field = taxonomy_csv_term_set_field_values($term, 'relations', $langcode);
    $term->taxonomy_relation[$langcode] = ($field) ?
      array_shift($field) :
      array();
    unset($term->relations);
  }

  return $term;
}

/**
 * Calculate number of terms in a vocabulary or in all vocabularies.
 *
 * @param $vocabulary_id
 *   Optional. Id of the chosen vocabulary. If not specified, count terms in all
 *   vocabularies.
 *
 * @return
 *   Number of terms in specified vocabulary or in all vocabularies.
 */
function taxonomy_csv_term_count_in_vocabulary($vocabulary_id = 0) {
  if ($vocabulary_id) {
    return count(taxonomy_term_load_multiple(
      array(),
      array('vid' => $vocabulary_id)
    ));
  }
  // Else.
  // @todo Use taxonomy api or regular Drupal 7 query.
  $sql = "
    SELECT COUNT(*) AS count
    FROM {taxonomy_term_data}
  ";
  $args = array();
  $result = db_query($sql, $args);
  foreach ($result as $item) {
    return $item->count;
  }
}
